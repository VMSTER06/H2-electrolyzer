package by.h2pro.control;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ServiceInfo;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/**
 * v1.2: фоновая проверка мозга H2 PRO и уведомления об авариях.
 *
 * Раз в 10 секунд читает http://192.168.4.1/status через сеть Wi-Fi H2_PRO (запрос
 * идёт именно в эту сеть, даже если включён мобильный интернет) и показывает
 * уведомление при ПОЯВЛЕНИИ аварии или предупреждения. Повторно об одной и той же
 * аварии не напоминает, пока она не пропадёт. Работает как foreground-служба
 * (постоянное тихое уведомление «Наблюдение»), включается в настройках приложения.
 */
public class H2MonitorService extends Service {
    private static final String PREFS = "h2.monitor";
    private static final String KEY_ON = "enabled";
    private static final String CH_WATCH = "h2.watch";
    private static final String CH_ALARM = "h2.alarm";
    private static final int ID_WATCH = 1;
    private static final long PERIOD_MS = 10_000;
    private static final String URL_STATUS = "http://192.168.4.1/status";

    // Аварии и предупреждения: ключ JSON статуса → текст уведомления
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
    };

    private HandlerThread thread;
    private Handler handler;
    private final boolean[] active = new boolean[FLAGS.length];
    private boolean everOnline = false, offlineNotified = false;
    private int failures = 0;

    public static boolean isEnabled(Context c) {
        return c.getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_ON, false);
    }

    public static void setEnabled(Context c, boolean on) {
        c.getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_ON, on).apply();
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
        }
        Notification watch = build(CH_WATCH, "H2 PRO — наблюдение", "Проверка каждые 10 с", false);
        if (Build.VERSION.SDK_INT >= 34)
            startForeground(ID_WATCH, watch, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else
            startForeground(ID_WATCH, watch);
        thread = new HandlerThread("h2-monitor");
        thread.start();
        handler = new Handler(thread.getLooper());
        handler.post(this::tick);
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (!isEnabled(this)) { stopSelf(); return START_NOT_STICKY; }
        return START_STICKY;
    }

    @Override public void onDestroy() {
        if (thread != null) thread.quitSafely();
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }

    private void tick() {
        if (!isEnabled(this)) { stopSelf(); return; }
        try {
            JSONObject d = new JSONObject(fetch());
            failures = 0;
            everOnline = true;
            offlineNotified = false;
            for (int i = 0; i < FLAGS.length; i++) {
                boolean on = d.optInt(FLAGS[i][0], 0) != 0;
                if (on && !active[i]) alert(100 + i, FLAGS[i][1], detail(FLAGS[i][0], d));
                active[i] = on;
            }
            String state = String.format(Locale.forLanguageTag("ru"), "%.2f бар · %s",
                d.optDouble("p", 0), d.optInt("pwr", 0) != 0 ? "работает" : "ожидание");
            getSystemService(NotificationManager.class)
                .notify(ID_WATCH, build(CH_WATCH, "H2 PRO — наблюдение", state, false));
        } catch (Exception e) {
            failures++;
            if (everOnline && failures >= 3 && !offlineNotified) {
                offlineNotified = true;
                alert(99, "Нет связи с мозгом H2 PRO", "Телефон не в сети H2_PRO или мозг выключен");
            }
        }
        handler.postDelayed(this::tick, PERIOD_MS);
    }

    private static String detail(String key, JSONObject d) {
        switch (key) {
            case "ovl": return String.format(Locale.US, "%.2f бар", d.optDouble("p", 0));
            case "tlk": return d.optInt("ta", 0) + "°";
            case "oh":  return String.format(Locale.US, "%.0f °C", d.optDouble("t", 0));
            case "psf": return String.format(Locale.US, "%.2f В на входе датчика", d.optDouble("pv", 0));
            default:    return "Откройте приложение H2 PRO";
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
        b.setSmallIcon(R.drawable.ic_launcher).setContentTitle(title).setContentText(text)
            .setContentIntent(pi).setOnlyAlertOnce(!alarm).setOngoing(!alarm).setAutoCancel(alarm)
            .setShowWhen(alarm);
        if (alarm) b.setCategory(Notification.CATEGORY_ALARM);
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

    private String fetch() throws Exception {
        Network n = brainNetwork();
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
