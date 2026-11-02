package by.h2pro.control;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.SystemClock;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * v1.2: фоновая проверка мозга H2 PRO и уведомления об авариях.
 *
 * Раз в 10 секунд читает http://192.168.4.1/status через сеть Wi-Fi H2_PRO (запрос
 * идёт именно в эту сеть, даже если включён мобильный интернет) и показывает
 * уведомление при ПОЯВЛЕНИИ аварии или предупреждения. Повторно об одной и той же
 * аварии не напоминает, пока она не пропадёт. Работает как foreground-служба
 * (постоянное тихое уведомление «Наблюдение»), включается в настройках приложения.
 *
 * v1.3.0: пока телефон в сети H2_PRO, служба держит процессор (WakeLock) — иначе при
 * погасшем экране Handler (uptime) засыпал вместе с процессором и тик уходил на минуты.
 */
public class H2MonitorService extends Service {
    private static final String PREFS = "h2.monitor";
    private static final String KEY_ON = "enabled";
    private static final String CH_WATCH = "h2.watch";
    static final String CH_ALARM = "h2.alarm";
    // v1.3.0: «Нет связи» — свой канал, но такой же громкий, как аварии (всплывает, вибрирует).
    // Новый id: важность уже созданного канала h2.link (IMPORTANCE_DEFAULT) поднять нельзя.
    private static final String CH_LINK = "h2.link.alarm";
    private static final String CH_LINK_OLD = "h2.link";
    private static final int ID_WATCH = 1;
    private static final int ID_STOPPED = 98;
    private static final int ID_LINK = 99;
    private static final long PERIOD_MS = 10_000;
    private static final long IDLE_MS = 60_000;          // вне сети H2_PRO и без незакрытой потери связи
    private static final long WAKE_MS = 60_000;          // удержание продлевается каждым тиком
    private static final String URL_STATUS = "http://192.168.4.1/status";
    // v1.3.0: «ovw» мозг отдаёт как P > 6.0 бар без гистерезиса — у цели 6.0 он дребезжит.
    // Как у звука мозга (WARN_PRESSURE − SND_WARN_REARM_BAR): флаг держим, пока P ≥ 5.9 бар,
    // а повторно оповещаем, только если предупреждения не было OVW_REARM_MS подряд.
    private static final String OVW = "ovw";
    private static final double OVW_CLEAR_BAR = 5.9;
    private static final long OVW_REARM_MS = 10 * 60_000;

    // Аварии и предупреждения: ключ JSON статуса → текст уведомления
    // (v1.3.0: новые ключи — в конце, номера уведомлений прежних не сдвигаются)
    private static final String[][] FLAGS = {
        {"ovl",  "Авария: давление выше допустимого"},
        {"tlk",  "Авария: наклон установки"},
        {"psf",  "Датчик давления неисправен — электролизёр заблокирован"},
        {"lkl",  "Утечка — система выключена"},
        {"oh",   "Перегрев"},
        {"lkw",  "Давление падает при закрытом клапане — возможна утечка"},
        {"wd",   "Бак не набирает давление"},
        {"mntd", "Пора обслужить: долить воду / электролит"},
        {"tme",  "Таймер выключил систему"},
        {"mpuf", "Датчик наклона не отвечает — электролизёр заблокирован"},
        {"ovw",  "Давление приближается к аварийному"},
        {"relw", "Ресурс реле на исходе — проверь контакты"},
    };

    private HandlerThread thread;
    private Handler handler;
    private PowerManager.WakeLock wl;
    private ConnectivityManager.NetworkCallback netCallback;
    // v1.3.0: tick после onDestroy не должен заново публиковать ongoing-уведомление
    private final Object lock = new Object();
    private boolean destroyed;                   // под lock
    private boolean fgFailed = false;
    private final boolean[] active = new boolean[FLAGS.length];
    private long ovwClearedAt = -1;              // elapsedRealtime снятия ovw; −1 — ещё не оповещали
    private boolean everOnline = false, offlineNotified = false;
    private int failures = 0;
    private long lastOkMs = 0;
    private String lastWatchText = "";

    public static boolean isEnabled(Context c) {
        return c.getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_ON, false);
    }

    public static void setEnabled(Context c, boolean on) {
        c.getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_ON, on).apply();
    }

    /**
     * v1.3.0: система действительно покажет аварийное уведомление: уведомления приложения
     * разрешены (на Android 13+ это и разрешение POST_NOTIFICATIONS) и канал аварий не
     * отключён. Канала ещё нет (служба не запускалась) — это нормально.
     */
    public static boolean notificationsUsable(Context c) {
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        if (nm == null || !nm.areNotificationsEnabled()) return false;
        if (Build.VERSION.SDK_INT < 26) return true;
        NotificationChannel ch = nm.getNotificationChannel(CH_ALARM);
        return ch == null || ch.getImportance() != NotificationManager.IMPORTANCE_NONE;
    }

    public static void start(Context c) {
        Intent i = new Intent(c, H2MonitorService.class);
        if (Build.VERSION.SDK_INT >= 26) c.startForegroundService(i);
        else c.startService(i);
    }

    @Override public void onCreate() {
        super.onCreate();
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(new NotificationChannel(CH_WATCH, "Наблюдение за H2 PRO",
                NotificationManager.IMPORTANCE_MIN));
            NotificationChannel alarm = new NotificationChannel(CH_ALARM, "Аварии H2 PRO",
                NotificationManager.IMPORTANCE_HIGH);
            alarm.enableVibration(true);
            nm.createNotificationChannel(alarm);
            // v1.3.0: связь — отдельный канал (его можно приглушить, не отключая настоящие
            // аварии), но по умолчанию так же заметный: потеря связи — потеря наблюдения
            NotificationChannel link = new NotificationChannel(CH_LINK, "Связь с H2 PRO",
                NotificationManager.IMPORTANCE_HIGH);
            link.enableVibration(true);
            nm.createNotificationChannel(link);
            nm.deleteNotificationChannel(CH_LINK_OLD);   // тихий канал из ранней сборки 1.3.0
        }
        Notification watch = build(CH_WATCH, "H2 PRO — наблюдение", "Проверка каждые 10 с", false);
        // v1.3.0: при sticky-перезапуске из фона Android 12+ может запретить startForeground
        // (ForegroundServiceStartNotAllowedException — наследник IllegalStateException)
        try {
            if (Build.VERSION.SDK_INT >= 34)
                startForeground(ID_WATCH, watch, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            else
                startForeground(ID_WATCH, watch);
        } catch (IllegalStateException e) {
            fgFailed = true;
            alert(ID_STOPPED, "Наблюдение H2 PRO остановлено системой", "Открой приложение, чтобы возобновить");
            stopSelf();
            return;
        }
        nm.cancel(ID_STOPPED);
        PowerManager pm = getSystemService(PowerManager.class);
        wl = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "h2pro:monitor");
        wl.setReferenceCounted(false);
        thread = new HandlerThread("h2-monitor");
        thread.start();
        handler = new Handler(thread.getLooper());
        // v1.3.0: вернулись в Wi-Fi (у сети H2_PRO нет интернета) — опрос сразу, а не через
        // отложенный «uptime»-тик, который при спящем процессоре мог ждать минуты
        ConnectivityManager cm = getSystemService(ConnectivityManager.class);
        if (cm != null) {
            netCallback = new ConnectivityManager.NetworkCallback() {
                @Override public void onAvailable(Network network) {
                    handler.removeCallbacksAndMessages(null);
                    handler.post(H2MonitorService.this::tick);
                }
            };
            try {
                cm.registerNetworkCallback(new NetworkRequest.Builder()
                    .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                    .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build(), netCallback, handler);
            } catch (RuntimeException e) {
                netCallback = null;
            }
        }
        handler.post(this::tick);
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (fgFailed) { stopSelf(); return START_NOT_STICKY; }
        if (!isEnabled(this)) { stopSelf(); return START_NOT_STICKY; }
        return START_STICKY;
    }

    @Override public void onDestroy() {
        synchronized (lock) { destroyed = true; }
        if (netCallback != null) {
            try { getSystemService(ConnectivityManager.class).unregisterNetworkCallback(netCallback); }
            catch (RuntimeException ignored) { }
            netCallback = null;
        }
        if (handler != null) handler.removeCallbacksAndMessages(null);
        if (wl != null && wl.isHeld()) wl.release();
        stopForeground(STOP_FOREGROUND_REMOVE);
        getSystemService(NotificationManager.class).cancel(ID_WATCH);
        if (thread != null) thread.quitSafely();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    private void tick() {
        if (!isEnabled(this)) { stopSelf(); return; }
        Network n = brainNetwork();
        boolean keep;
        synchronized (lock) {
            if (destroyed) return;
            // v1.3.0: держать процессор, пока телефон в сети H2_PRO или ещё не сообщили о потере
            // связи. Тайм-аут продлевается каждым тиком и сам снимет удержание, если служба умрёт.
            keep = n != null || (everOnline && !offlineNotified);
            if (keep) wl.acquire(WAKE_MS);
            else if (wl.isHeld()) wl.release();
        }
        JSONObject d = null;
        try {
            d = new JSONObject(fetch(n));   // долгий запрос — вне lock: onDestroy не ждёт его
        } catch (Exception ignored) { }
        synchronized (lock) {
            if (destroyed) return;
            if (!isEnabled(this)) { stopSelf(); return; }
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (d != null) {
                failures = 0;
                everOnline = true;
                lastOkMs = System.currentTimeMillis();
                if (offlineNotified) nm.cancel(ID_LINK);   // v1.3.0: связь вернулась — снять «Нет связи»
                offlineNotified = false;
                long now = SystemClock.elapsedRealtime();
                for (int i = 0; i < FLAGS.length; i++) {
                    boolean on = d.optInt(FLAGS[i][0], 0) != 0;
                    if (OVW.equals(FLAGS[i][0])) {
                        // v1.3.0: гистерезис 0,1 бар — снимаем только при P < 5.9 бар
                        if (!on && active[i] && d.optDouble("p", 0) >= OVW_CLEAR_BAR) on = true;
                        if (!on && active[i]) ovwClearedAt = now;
                        // вернулось вскоре после снятия (регулятор у цели, расход газа) — без нового звонка
                        if (on && !active[i] && ovwClearedAt >= 0 && now - ovwClearedAt < OVW_REARM_MS) {
                            active[i] = true;
                            continue;
                        }
                    }
                    if (on && !active[i]) alert(100 + i, FLAGS[i][1], detail(FLAGS[i][0], d));
                    active[i] = on;
                }
                watch(nm, String.format(Locale.forLanguageTag("ru"), "%.2f бар · %s",
                    d.optDouble("p", 0), d.optInt("pwr", 0) != 0 ? "работает" : "ожидание"));
            } else {
                failures++;
                if (everOnline && failures >= 3 && !offlineNotified) {
                    offlineNotified = true;
                    nm.notify(ID_LINK, build(CH_LINK, "Нет связи с мозгом H2 PRO",
                        "Телефон не в сети H2_PRO или мозг выключен", true));
                }
                // v1.3.0: старые показания не выдаём за текущие (тот же порог — без мигания)
                if (failures >= 3) watch(nm, lastOkMs == 0 ? "Нет связи" : "Нет связи · последний ответ "
                    + new SimpleDateFormat("HH:mm", Locale.getDefault()).format(new Date(lastOkMs)));
            }
            handler.postDelayed(this::tick, keep ? PERIOD_MS : IDLE_MS);
        }
    }

    /** Обновить постоянное уведомление, только если текст изменился. */
    private void watch(NotificationManager nm, String text) {
        if (text.equals(lastWatchText)) return;
        lastWatchText = text;
        nm.notify(ID_WATCH, build(CH_WATCH, "H2 PRO — наблюдение", text, false));
    }

    private static String detail(String key, JSONObject d) {
        switch (key) {
            case "ovl": return String.format(Locale.US, "%.2f бар", d.optDouble("p", 0));
            case "ovw": return String.format(Locale.US, "%.2f бар (авария при 6.8 бар)", d.optDouble("p", 0));
            case "tlk": return d.optInt("ta", 0) + "°";
            case "oh":  return String.format(Locale.US, "%.0f °C", d.optDouble("t", 0));
            case "psf": return String.format(Locale.US, "%.2f В на входе датчика", d.optDouble("pv", 0));
            default:    return "Открой приложение H2 PRO";
        }
    }

    private void alert(int id, String title, String text) {
        getSystemService(NotificationManager.class).notify(id, build(CH_ALARM, title, text, true));
    }

    private Notification build(String channel, String title, String text, boolean alarm) {
        Intent open = new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b = Build.VERSION.SDK_INT >= 26
            ? new Notification.Builder(this, channel) : new Notification.Builder(this);
        // v1.3.0: монохромная иконка «H2» — система рисует её по альфа-каналу
        b.setSmallIcon(R.drawable.ic_stat_h2).setContentTitle(title).setContentText(text)
            .setContentIntent(pi).setOnlyAlertOnce(!alarm).setOngoing(!alarm).setAutoCancel(alarm)
            .setShowWhen(alarm);
        if (alarm && (CH_ALARM.equals(channel) || CH_LINK.equals(channel)))
            b.setCategory(Notification.CATEGORY_ALARM).setColor(0xFFA33B2B);
        return b.build();
    }

    /** Сеть Wi-Fi мозга (адрес 192.168.4.x) или null. */
    private Network brainNetwork() {
        ConnectivityManager cm = getSystemService(ConnectivityManager.class);
        if (cm == null) return null;
        for (Network n : cm.getAllNetworks()) {
            NetworkCapabilities caps = cm.getNetworkCapabilities(n);
            LinkProperties links = cm.getLinkProperties(n);
            if (caps == null || links == null || !caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) continue;
            for (LinkAddress a : links.getLinkAddresses()) {
                if (!(a.getAddress() instanceof Inet4Address)) continue;
                byte[] ip = a.getAddress().getAddress();
                if ((ip[0] & 255) == 192 && (ip[1] & 255) == 168 && (ip[2] & 255) == 4) return n;
            }
        }
        return null;
    }

    private String fetch(Network n) throws Exception {
        if (n == null) throw new IllegalStateException("нет сети H2_PRO");
        HttpURLConnection c = (HttpURLConnection) n.openConnection(new URL(URL_STATUS));
        try {
            c.setConnectTimeout(3000);
            c.setReadTimeout(4000);
            c.setUseCaches(false);
            c.setRequestProperty("Connection", "close");
            if (c.getResponseCode() != 200) throw new IllegalStateException("HTTP " + c.getResponseCode());
            try (InputStream in = c.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                byte[] buf = new byte[2048];
                int k;
                while ((k = in.read(buf)) != -1 && out.size() < 32768) out.write(buf, 0, k);
                return out.toString(StandardCharsets.UTF_8.name());
            }
        } finally {
            c.disconnect();
        }
    }
}
