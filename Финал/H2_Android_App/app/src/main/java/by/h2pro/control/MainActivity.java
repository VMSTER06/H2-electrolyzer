package by.h2pro.control;

import android.Manifest;
import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Base64;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

public class MainActivity extends Activity {
    private static final String BASE_URL = "http://192.168.4.1";
    private static final int REQ_NOTIFY = 1, REQ_FILE = 10, REQ_SAVE = 11;

    // v1.3.0: четыре очереди. Опрос (/status, журнал, настройки звука, часы) не стоит за
    // командами, команды — за опросом, а долгие загрузки (мелодия, прошивка) — ни за кем.
    // «Сброс давления» (99) — своя очередь: аварийная команда не ждёт никого.
    private static final int LANE_POLL = 0, LANE_CMD = 1, LANE_UPLOAD = 2, LANE_SOS = 3;
    // Предел ожидания в очереди: команда, простоявшая дольше, НЕ отправляется — иначе она
    // могла уйти уже после того, как JS сообщил «не ответило» (переключатели 1/2/7 сработали
    // бы дважды). Инвариант: JS-таймаут > очередь + connect + read
    //   команды: 8 с > 1 + 2,5 + 3 = 6,5 с;  опрос: 12 с > 4 + 2,5 + 4,5 = 11 с.
    // 99 по возрасту не отбрасывается: продувка до нуля идемпотентна, лучше поздно, чем никогда.
    private static final long CMD_QUEUE_MS = 1000, POLL_QUEUE_MS = 4000;
    private static final int CONNECT_MS = 2500, CMD_READ_MS = 3000, POLL_READ_MS = 4500;
    private static final int MEL_READ_MS = 10_000, OTA_READ_MS = 60_000;
    private static final int MAX_MELODY_BODY = 16_384;       // H2M1 ≤ 4855 байт + multipart
    private static final int MAX_FIRMWARE_BODY = 2_097_152;  // раздел app 1,25 МБ + запас
    private static final List<Integer> COMMANDS = Arrays.asList(1, 2, 3, 4, 5, 7, 8, 9, 12, 99);

    private WebView webView;
    private ConnectivityManager connectivity;
    private final ExecutorService pollIo = Executors.newSingleThreadExecutor();
    // По потоку на каждый одновременный запрос JS: обычное действие (actionBusy) + 2 + 12
    // (+ запас) — команда не простаивает в очереди и не отбрасывается по CMD_QUEUE_MS
    private final ExecutorService cmdIo = Executors.newFixedThreadPool(4);
    private final ExecutorService sosIo = Executors.newSingleThreadExecutor();   // только /cmd?id=99
    private final ExecutorService upIo = Executors.newSingleThreadExecutor();
    private ValueCallback<Uri[]> fileCallback;
    private int saveId;
    private byte[] saveData;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        connectivity = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        webView = new WebView(this);
        // v1.3.0: фон по системной теме — без светлой вспышки в тёмной до appearance()
        webView.setBackgroundColor(nightMode() ? 0xFF171A18 : 0xFFF1F0EC);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.getSettings().setDomStorageEnabled(true);
        webView.getSettings().setAllowFileAccess(true);
        webView.getSettings().setAllowFileAccessFromFileURLs(false);
        webView.getSettings().setAllowUniversalAccessFromFileURLs(false);
        webView.getSettings().setMediaPlaybackRequiresUserGesture(false);   // WebAudio: прослушивание мелодий
        webView.addJavascriptInterface(new BrainApi(), "Brain");
        webView.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return !"file".equals(request.getUrl().getScheme());
            }
        });
        // v1.3.0: выбор файла для <input type=file> (мелодии, прошивка, импорт истории)
        webView.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback,
                                                       FileChooserParams params) {
                if (fileCallback != null) fileCallback.onReceiveValue(null);
                fileCallback = callback;
                String[] actions = { Intent.ACTION_OPEN_DOCUMENT, Intent.ACTION_GET_CONTENT };
                for (String a : actions) {
                    try {
                        startActivityForResult(new Intent(a).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"), REQ_FILE);
                        return true;
                    } catch (RuntimeException ignored) { }
                }
                fileCallback = null;
                return false;
            }
        });
        setContentView(webView);
        webView.loadUrl("file:///android_asset/index.html");
        if (H2MonitorService.isEnabled(this)) H2MonitorService.start(this);   // v1.2: уведомления
    }

    private boolean nightMode() {
        return (getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK)
            == Configuration.UI_MODE_NIGHT_YES;
    }

    private boolean bindBrainWifi() {
        // Вызывается из нескольких потоков: привязка к той же сети идемпотентна
        if (connectivity == null) return false;
        for (Network network : connectivity.getAllNetworks()) {
            NetworkCapabilities caps = connectivity.getNetworkCapabilities(network);
            LinkProperties links = connectivity.getLinkProperties(network);
            if (caps == null || links == null || !caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) continue;
            for (LinkAddress address : links.getLinkAddresses()) {
                if (!(address.getAddress() instanceof Inet4Address)) continue;
                byte[] ip = address.getAddress().getAddress();
                if ((ip[0] & 255) == 192 && (ip[1] & 255) == 168 && (ip[2] & 255) == 4) {
                    return connectivity.bindProcessToNetwork(network);
                }
            }
        }
        connectivity.bindProcessToNetwork(null);
        return false;
    }

    // ── v1.3.0: белый список путей мозга. Возвращает очередь или −1 (запрос отклонён).
    //    Проверки диапазонов — те же, что в прошивке; защитные (команды, цель давления,
    //    калибровка, таймер, вентилятор) не дают отправить мусор даже при ошибке в JS.
    static int route(String method, String path, String contentType, int bodyLength) {
        if (path == null || path.length() > 300 || !path.startsWith("/") || path.startsWith("//")) return -1;
        for (int i = 0; i < path.length(); i++) {
            char c = path.charAt(i);
            if (c <= ' ' || c >= 127 || c == '#' || c == '\\' || c == '@') return -1;
        }
        Uri u = Uri.parse(BASE_URL + path);
        if (!"192.168.4.1".equals(u.getHost()) || u.getPort() != -1 || u.getPath() == null) return -1;
        String ep = u.getPath();
        if ("POST".equals(method)) {
            if (contentType == null || u.getQuery() != null
                || !contentType.matches("multipart/form-data; boundary=[A-Za-z0-9_.\\-]{1,70}")) return -1;
            if ("/snd/up".equals(ep)) return bodyLength > 0 && bodyLength <= MAX_MELODY_BODY ? LANE_UPLOAD : -1;
            if ("/update".equals(ep)) return bodyLength > 0 && bodyLength <= MAX_FIRMWARE_BODY ? LANE_UPLOAD : -1;
            return -1;
        }
        if (!"GET".equals(method) || bodyLength != 0) return -1;
        Set<String> q = u.getQueryParameterNames();
        switch (ep) {
            case "/status": case "/log": case "/snd/cfg":
                return q.isEmpty() ? LANE_POLL : -1;
            case "/snd/stop": case "/log/clear": case "/resetcycles": case "/resetruntime":
                return q.isEmpty() ? LANE_CMD : -1;
            case "/settime":
                return only(q, "t") && intIn(u, "t", 1_700_000_000L, 4_294_967_295L) ? LANE_POLL : -1;
            case "/snd/get":
                return only(q, "id") && melodyId(u, "id") ? LANE_POLL : -1;
            case "/cmd": {
                Long c = intParam(u, "id");
                if (!only(q, "id") || c == null || c < 0 || c > 99 || !COMMANDS.contains(c.intValue())) return -1;
                return c == 99 ? LANE_SOS : LANE_CMD;
            }
            case "/setpressure":
                return only(q, "val") && numIn(u, "val", 0.5, 6.5) ? LANE_CMD : -1;
            case "/settimer":
                return only(q, "min") && intIn(u, "min", 0, 600) ? LANE_CMD : -1;
            case "/setgas":
                return (only(q, "tank") && numIn(u, "tank", 0.1, 100))
                    || (only(q, "reset") && intIn(u, "reset", 1, 1)) ? LANE_CMD : -1;
            case "/maint":
                return (only(q, "h") && intIn(u, "h", 1, 1000))
                    || (only(q, "done") && intIn(u, "done", 1, 1)) ? LANE_CMD : -1;
            case "/setfans": {
                Long on = intParam(u, "ton"), off = intParam(u, "toff");
                return only(q, "i", "mode", "ton", "toff") && intIn(u, "i", 2, 2) && intIn(u, "mode", 0, 3)
                    && on != null && off != null && on >= 20 && on <= 90 && off >= 15 && off < on ? LANE_CMD : -1;
            }
            case "/calibrate":
                return only(q, "offset", "scale") && numIn(u, "offset", 0, 2) && numIn(u, "scale", 0.1, 10)
                    ? LANE_CMD : -1;
            case "/snd/set": {
                boolean events = q.contains("boot") || q.contains("on");
                boolean names = events ? only(q, "out", "vol", "alm", "clk", "boot", "on")
                                       : only(q, "out", "vol", "alm", "clk");
                return names && intIn(u, "out", 0, 3) && intIn(u, "vol", 1, 10) && intIn(u, "alm", 0, 1)
                    && intIn(u, "clk", 0, 1) && (!events || (eventId(u, "boot") && eventId(u, "on"))) ? LANE_CMD : -1;
            }
            case "/snd/play":
                return (only(q, "id") || (only(q, "id", "out") && intIn(u, "out", -1, 3)))
                    && melodyId(u, "id") ? LANE_CMD : -1;
            case "/snd/del":
                return only(q, "id") && intIn(u, "id", 0, 15) ? LANE_CMD : -1;
            default:
                return -1;
        }
    }

    private static boolean only(Set<String> q, String... names) {
        return q.size() == names.length && q.containsAll(Arrays.asList(names));
    }

    /** Ровно одно значение параметра, иначе null (повтор id=1&id=99 не пропускаем). */
    private static String param(Uri u, String name) {
        List<String> v = u.getQueryParameters(name);
        return v.size() == 1 ? v.get(0) : null;
    }

    private static Long intParam(Uri u, String name) {
        String s = param(u, name);
        if (s == null || !s.matches("-?\\d{1,10}")) return null;
        return Long.parseLong(s);
    }

    private static boolean intIn(Uri u, String name, long min, long max) {
        Long v = intParam(u, name);
        return v != null && v >= min && v <= max;
    }

    private static boolean numIn(Uri u, String name, double min, double max) {
        String s = param(u, name);
        if (s == null || !s.matches("-?\\d{1,4}(\\.\\d{1,4})?")) return false;
        double v = Double.parseDouble(s);
        return Double.isFinite(v) && v >= min && v <= max;
    }

    private static boolean melodyId(Uri u, String name) {
        return intIn(u, name, -3, -1) || intIn(u, name, 0, 15);
    }

    private static boolean eventId(Uri u, String name) {
        Long v = intParam(u, name);
        return v != null && (v == -9 || v == -1 || v == -2 || (v >= 0 && v <= 15));
    }

    private void send(int id, int lane, String method, String path, String contentType, byte[] body) {
        final long queued = SystemClock.elapsedRealtime();
        ExecutorService ex = lane == LANE_POLL ? pollIo : lane == LANE_CMD ? cmdIo : lane == LANE_SOS ? sosIo : upIo;
        try {
            ex.execute(() -> {
                long waited = SystemClock.elapsedRealtime() - queued;
                if (lane == LANE_CMD && waited > CMD_QUEUE_MS) {
                    respond(id, 504, "Команда не отправлена: мозг не отвечает. Проверь Wi-Fi H2_PRO и повтори", false);
                    return;
                }
                if (lane == LANE_POLL && waited > POLL_QUEUE_MS) {
                    respond(id, 504, "Мозг не ответил. Проверь Wi-Fi H2_PRO и повтори", false);
                    return;
                }
                exchange(id, lane, method, path, contentType, body);
            });
        } catch (RejectedExecutionException ignored) {
            // активность закрывается — отвечать уже некому
        }
    }

    private void exchange(int id, int lane, String method, String path, String contentType, byte[] body) {
        int code = -1;
        String text;
        boolean base64 = false;
        if (!bindBrainWifi()) {
            text = "Подключи телефон к Wi-Fi H2_PRO";
        } else {
            HttpURLConnection connection = null;
            try {
                connection = (HttpURLConnection) new URL(BASE_URL + path).openConnection();
                connection.setRequestMethod(method);
                connection.setConnectTimeout(CONNECT_MS);
                connection.setReadTimeout(lane == LANE_CMD || lane == LANE_SOS ? CMD_READ_MS : lane == LANE_POLL ? POLL_READ_MS
                    : path.startsWith("/update") ? OTA_READ_MS : MEL_READ_MS);
                connection.setUseCaches(false);
                connection.setInstanceFollowRedirects(false);
                connection.setRequestProperty("Connection", "close");
                if (body != null) {
                    connection.setDoOutput(true);
                    connection.setRequestProperty("Content-Type", contentType);
                    connection.setFixedLengthStreamingMode(body.length);
                    try (OutputStream out = connection.getOutputStream()) {
                        long noted = 0;
                        for (int off = 0; off < body.length; ) {
                            int n = Math.min(8192, body.length - off);
                            out.write(body, off, n);
                            off += n;
                            long now = SystemClock.elapsedRealtime();
                            if (now - noted >= 200 || off == body.length) { noted = now; progress(id, off, body.length); }
                        }
                        out.flush();
                    }
                }
                code = connection.getResponseCode();
                InputStream stream = code >= 400 ? connection.getErrorStream() : connection.getInputStream();
                String type = connection.getContentType();
                if (stream == null) text = "Мозг прислал пустой ответ. Повтори";
                else try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                    byte[] buffer = new byte[4096];
                    int n;
                    while ((n = input.read(buffer)) != -1 && output.size() < 65536) output.write(buffer, 0, n);
                    if (code < 400 && type != null && type.startsWith("application/octet-stream")) {
                        text = Base64.encodeToString(output.toByteArray(), Base64.NO_WRAP);   // /snd/get: H2M1
                        base64 = true;
                    } else {
                        text = output.toString(StandardCharsets.UTF_8.name());
                    }
                }
            } catch (Exception e) {
                // v1.3.0: без имени класса исключения — пользователю нужен понятный следующий шаг
                text = "Мозг не ответил. Проверь Wi-Fi H2_PRO и повтори";
            } finally {
                if (connection != null) connection.disconnect();
            }
        }
        respond(id, code, text, base64);
    }

    private void respond(int id, int code, String body, boolean base64) {
        runOnUiThread(() -> {
            if (webView != null) webView.evaluateJavascript(
                "window.h2Response(" + id + "," + code + "," + JSONObject.quote(body) + "," + base64 + ")", null);
        });
    }

    private void progress(int id, int sent, int total) {
        runOnUiThread(() -> {
            if (webView != null) webView.evaluateJavascript(
                "window.h2Progress&&h2Progress(" + id + "," + sent + "," + total + ")", null);
        });
    }

    private void monitorResult(boolean ok) {
        if (webView != null) webView.evaluateJavascript("window.h2MonitorResult&&h2MonitorResult(" + ok + ")", null);
    }

    /** Системные настройки уведомлений: канал аварий, если отключён он, иначе — приложение. */
    private void openNotificationSettingsNow() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannel alarm = nm != null ? nm.getNotificationChannel(H2MonitorService.CH_ALARM) : null;
        Intent i = nm != null && nm.areNotificationsEnabled() && alarm != null
                && alarm.getImportance() == NotificationManager.IMPORTANCE_NONE
            ? new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS)
                .putExtra(Settings.EXTRA_CHANNEL_ID, H2MonitorService.CH_ALARM)
            : new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
        i.putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName());
        try { startActivity(i); return; } catch (RuntimeException ignored) { }
        try {
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.fromParts("package", getPackageName(), null)));
        } catch (RuntimeException ignored) { }
    }

    public class BrainApi {
        @JavascriptInterface public void appearance(boolean dark, boolean keepAwake) {
            runOnUiThread(() -> {
                int background = Color.parseColor(dark ? "#171a18" : "#f1f0ec");
                getWindow().setStatusBarColor(background);
                getWindow().setNavigationBarColor(background);
                getWindow().getDecorView().setSystemUiVisibility(dark ? 0 :
                    View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
                if (webView != null) webView.setBackgroundColor(background);
                if (keepAwake) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            });
        }

        @JavascriptInterface public void haptic() {
            runOnUiThread(() -> {
                if (webView != null) webView.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            });
        }

        @JavascriptInterface public void openWifiSettings() {
            runOnUiThread(() -> {
                // ⚡ v1.3.0: на прошивках без экрана Wi-Fi не падаем — пробуем запасные экраны, иначе подсказка в WebView
                String[] actions = { Settings.ACTION_WIFI_SETTINGS, Settings.ACTION_WIRELESS_SETTINGS, Settings.ACTION_SETTINGS };
                for (String a : actions) {
                    try { startActivity(new Intent(a)); return; } catch (RuntimeException ignored) { }
                }
                if (webView != null) webView.evaluateJavascript(
                    "window.toast&&toast('Открой Wi-Fi телефона и выбери H2_PRO')", null);
            });
        }

        /**
         * v1.3.0: единственный HTTP-метод моста. method — GET или POST; path — путь мозга с
         * параметрами (белый список и диапазоны — route()); для POST contentType —
         * multipart/form-data с границей, bodyBase64 — готовое тело. Ответ приходит в
         * window.h2Response(id, code, body, base64): бинарный ответ (/snd/get) — в Base64.
         */
        @JavascriptInterface public void request(int id, String method, String path, String contentType, String bodyBase64) {
            String m = method == null ? "" : method.toUpperCase(Locale.ROOT);
            byte[] body = null;
            if ("POST".equals(m)) {
                try {
                    body = Base64.decode(bodyBase64 == null ? "" : bodyBase64, Base64.DEFAULT);
                } catch (IllegalArgumentException e) {
                    reject(id);
                    return;
                }
            } else if (bodyBase64 != null && !bodyBase64.isEmpty()) {
                reject(id);
                return;
            }
            int lane = route(m, path, contentType, body == null ? 0 : body.length);
            if (lane < 0) { reject(id); return; }
            send(id, lane, m, path, contentType, body);
        }

        /** v1.3.0: экспорт истории — системный диалог «Сохранить как» (SAF). */
        @JavascriptInterface public void saveFile(int id, String name, String mime, String text) {
            if (name == null || text == null || !name.matches("[A-Za-z0-9_.\\-]{1,64}") || text.length() > 8_000_000
                || !("text/csv".equals(mime) || "application/json".equals(mime))) {
                reject(id);
                return;
            }
            runOnUiThread(() -> {
                if (saveData != null) respond(saveId, 499, "Отменено", false);
                saveId = id;
                saveData = text.getBytes(StandardCharsets.UTF_8);
                Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                    .setType(mime).putExtra(Intent.EXTRA_TITLE, name);
                try {
                    startActivityForResult(i, REQ_SAVE);
                } catch (RuntimeException e) {
                    saveData = null;
                    respond(id, 500, "Нет приложения для сохранения файлов", false);
                }
            });
        }

        // Фоновые уведомления об авариях (служба H2MonitorService)
        // v1.3.0: «включено» — только если уведомления действительно показываются
        @JavascriptInterface public boolean monitorEnabled() {
            return H2MonitorService.isEnabled(MainActivity.this) && H2MonitorService.notificationsUsable(MainActivity.this);
        }

        /** Наблюдение включено, но система не покажет уведомления (запрет или отключён канал). */
        @JavascriptInterface public boolean monitorBlocked() {
            return H2MonitorService.isEnabled(MainActivity.this) && !H2MonitorService.notificationsUsable(MainActivity.this);
        }

        @JavascriptInterface public void monitor(boolean on) {
            runOnUiThread(() -> {
                if (!on) {
                    H2MonitorService.setEnabled(MainActivity.this, false);
                    stopService(new Intent(MainActivity.this, H2MonitorService.class));
                    return;
                }
                // v1.3.0: без разрешения службу не запускаем — решение в onRequestPermissionsResult
                if (Build.VERSION.SDK_INT >= 33 &&
                    checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIFY);
                    return;
                }
                if (!H2MonitorService.notificationsUsable(MainActivity.this)) {
                    openNotificationSettingsNow();
                    monitorResult(false);
                    return;
                }
                H2MonitorService.setEnabled(MainActivity.this, true);
                H2MonitorService.start(MainActivity.this);
                monitorResult(true);
            });
        }

        @JavascriptInterface public void openNotificationSettings() {
            runOnUiThread(MainActivity.this::openNotificationSettingsNow);
        }

        /** v1.3.0: экран оптимизации батареи — чтобы система не душила наблюдение. */
        @JavascriptInterface public void openBatterySettings() {
            runOnUiThread(() -> {
                try { startActivity(new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS)); }
                catch (RuntimeException e) {
                    if (webView != null) webView.evaluateJavascript(
                        "window.toast&&toast('Открой настройки батареи телефона вручную',true)", null);
                }
            });
        }

        private void reject(int id) {
            runOnUiThread(() -> {
                if (webView != null) webView.evaluateJavascript(
                    "window.h2Response(" + id + ",400,\"Недопустимое значение\",false)", null);
            });
        }
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != REQ_NOTIFY) { super.onRequestPermissionsResult(requestCode, permissions, grantResults); return; }
        // v1.3.0: пустой grantResults (диалог прервали) — не отказ, а повод перепроверить
        boolean granted = Build.VERSION.SDK_INT < 33 ||
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED;
        if (granted && H2MonitorService.notificationsUsable(this)) {
            H2MonitorService.setEnabled(this, true);
            H2MonitorService.start(this);
            monitorResult(true);
            return;
        }
        H2MonitorService.setEnabled(this, false);
        stopService(new Intent(this, H2MonitorService.class));
        // «Больше не спрашивать» (диалог уже не появится) или запрещён канал — ведём в настройки
        boolean permanent = Build.VERSION.SDK_INT >= 33 && grantResults.length > 0
            && !shouldShowRequestPermissionRationale(Manifest.permission.POST_NOTIFICATIONS);
        if (granted || permanent) openNotificationSettingsNow();
        monitorResult(false);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQ_FILE) {
            ValueCallback<Uri[]> callback = fileCallback;
            fileCallback = null;
            if (callback != null) callback.onReceiveValue(resultCode == RESULT_OK && data != null && data.getData() != null
                ? new Uri[]{ data.getData() } : null);
            return;
        }
        if (requestCode == REQ_SAVE) {
            final int id = saveId;
            final byte[] bytes = saveData;
            saveData = null;
            if (resultCode != RESULT_OK || data == null || data.getData() == null || bytes == null) {
                respond(id, 499, "Отменено", false);
                return;
            }
            final Uri uri = data.getData();
            new Thread(() -> {
                try (OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
                    if (out == null) throw new IllegalStateException();
                    out.write(bytes);
                    respond(id, 200, "OK", false);
                } catch (Exception e) {
                    respond(id, 500, "Не удалось сохранить файл", false);
                }
            }, "h2-save").start();
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override public void onBackPressed() {
        if (webView == null) { super.onBackPressed(); return; }
        webView.evaluateJavascript("window.handleBack ? window.handleBack() : false", result -> {
            if (!"true".equals(result)) MainActivity.super.onBackPressed();
        });
    }

    @Override protected void onDestroy() {
        pollIo.shutdownNow();
        cmdIo.shutdownNow();
        sosIo.shutdownNow();
        upIo.shutdownNow();
        if (fileCallback != null) { fileCallback.onReceiveValue(null); fileCallback = null; }
        if (connectivity != null) connectivity.bindProcessToNetwork(null);
        if (webView != null) { webView.removeJavascriptInterface("Brain"); webView.destroy(); webView = null; }
        super.onDestroy();
    }
}
