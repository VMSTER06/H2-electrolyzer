package by.h2pro.control;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Bundle;
import android.provider.Settings;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.Inet4Address;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final String BASE_URL = "http://192.168.4.1";
    private WebView webView;
    private ConnectivityManager connectivity;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        connectivity = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        webView = new WebView(this);
        webView.setBackgroundColor(0xFFF1F0EC);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.getSettings().setDomStorageEnabled(true);
        webView.getSettings().setAllowFileAccess(true);
        webView.getSettings().setAllowFileAccessFromFileURLs(false);
        webView.getSettings().setAllowUniversalAccessFromFileURLs(false);
        webView.addJavascriptInterface(new BrainApi(), "Brain");
        webView.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return !"file".equals(request.getUrl().getScheme());
            }
        });
        setContentView(webView);
        webView.loadUrl("file:///android_asset/index.html");
        if (H2MonitorService.isEnabled(this)) H2MonitorService.start(this);   // v1.2: уведомления
    }

    private boolean bindBrainWifi() {
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

    private void get(int id, String path) {
        io.execute(() -> {
            int code = -1;
            String body;
            if (!bindBrainWifi()) {
                body = "Подключите телефон к Wi-Fi H2_PRO";
            } else {
                HttpURLConnection connection = null;
                try {
                    connection = (HttpURLConnection) new URL(BASE_URL + path).openConnection();
                    connection.setRequestMethod("GET");
                    connection.setConnectTimeout(2500);
                    connection.setReadTimeout(4500);
                    connection.setUseCaches(false);
                    connection.setRequestProperty("Connection", "close");
                    code = connection.getResponseCode();
                    InputStream stream = code >= 400 ? connection.getErrorStream() : connection.getInputStream();
                    if (stream == null) body = "Пустой ответ";
                    else try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                        byte[] buffer = new byte[2048];
                        int n;
                        while ((n = input.read(buffer)) != -1 && output.size() < 32768) output.write(buffer, 0, n);
                        body = output.toString(StandardCharsets.UTF_8.name());
                    }
                } catch (Exception e) {
                    body = "Мозг не отвечает: " + e.getClass().getSimpleName();
                } finally {
                    if (connection != null) connection.disconnect();
                }
            }
            final int resultCode = code;
            final String resultBody = body;
            runOnUiThread(() -> {
                if (webView != null) webView.evaluateJavascript(
                    "window.h2Response(" + id + "," + resultCode + "," + JSONObject.quote(resultBody) + ")", null);
            });
        });
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
            runOnUiThread(() -> startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS)));
        }

        @JavascriptInterface public void status(int id) { get(id, "/status"); }
        @JavascriptInterface public void soundConfig(int id) { get(id, "/snd/cfg"); }

        @JavascriptInterface public void command(int id, int code) {
            if (code == 1 || code == 2 || code == 3 || code == 4 || code == 5 ||
                code == 7 || code == 8 || code == 9 || code == 12 || code == 99) get(id, "/cmd?id=" + code);
            else reject(id);
        }

        // v1.2: таймер работы (0 = без ограничения), бак, обслуживание, журнал, часы
        @JavascriptInterface public void timer(int id, int minutes) {
            if (minutes >= 0 && minutes <= 600) get(id, "/settimer?min=" + minutes);
            else reject(id);
        }

        @JavascriptInterface public void gasTank(int id, double liters) {
            if (Double.isFinite(liters) && liters >= 0.1 && liters <= 100)
                get(id, "/setgas?tank=" + String.format(Locale.US, "%.2f", liters));
            else reject(id);
        }

        @JavascriptInterface public void maint(int id, int hours, int done) {
            if (done == 1) get(id, "/maint?done=1");
            else if (hours >= 1 && hours <= 1000) get(id, "/maint?h=" + hours);
            else reject(id);
        }

        @JavascriptInterface public void log(int id) { get(id, "/log"); }

        @JavascriptInterface public void syncTime(int id, double unixSeconds) {
            if (Double.isFinite(unixSeconds) && unixSeconds > 1.7e9)
                get(id, "/settime?t=" + (long) unixSeconds);
            else reject(id);
        }

        // Фоновые уведомления об авариях (служба H2MonitorService)
        @JavascriptInterface public boolean monitorEnabled() { return H2MonitorService.isEnabled(MainActivity.this); }

        @JavascriptInterface public void monitor(boolean on) {
            runOnUiThread(() -> {
                H2MonitorService.setEnabled(MainActivity.this, on);
                if (on && Build.VERSION.SDK_INT >= 33 &&
                    checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
                    requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 1);
                if (on) H2MonitorService.start(MainActivity.this);
                else stopService(new Intent(MainActivity.this, H2MonitorService.class));
            });
        }

        @JavascriptInterface public void pressure(int id, double value) {
            if (Double.isFinite(value) && value >= 0.5 && value <= 6.5)
                get(id, "/setpressure?val=" + String.format(Locale.US, "%.1f", value));
            else reject(id);
        }

        @JavascriptInterface public void fan(int id, int mode, int on, int off) {
            if (mode >= 0 && mode <= 3 && on >= 20 && on <= 90 && off >= 15 && off < on)
                get(id, "/setfans?i=2&mode=" + mode + "&ton=" + on + "&toff=" + off);
            else reject(id);
        }

        @JavascriptInterface public void soundSet(int id, int out, int volume, int alarms, int clicks) {
            if (out >= 0 && out <= 3 && volume >= 1 && volume <= 10 &&
                (alarms == 0 || alarms == 1) && (clicks == 0 || clicks == 1))
                get(id, "/snd/set?out=" + out + "&vol=" + volume + "&alm=" + alarms + "&clk=" + clicks);
            else reject(id);
        }

        @JavascriptInterface public void soundPlay(int id, int melody) {
            if ((melody >= -3 && melody <= -1) || (melody >= 0 && melody <= 15))
                get(id, "/snd/play?id=" + melody);
            else reject(id);
        }

        @JavascriptInterface public void soundStop(int id) { get(id, "/snd/stop"); }

        private void reject(int id) {
            runOnUiThread(() -> {
                if (webView != null) webView.evaluateJavascript(
                    "window.h2Response(" + id + ",400,\"Недопустимое значение\")", null);
            });
        }
    }

    @Override public void onBackPressed() {
        if (webView == null) { super.onBackPressed(); return; }
        webView.evaluateJavascript("window.handleBack ? window.handleBack() : false", result -> {
            if (!"true".equals(result)) MainActivity.super.onBackPressed();
        });
    }

    @Override protected void onDestroy() {
        io.shutdownNow();
        if (connectivity != null) connectivity.bindProcessToNetwork(null);
        if (webView != null) { webView.removeJavascriptInterface("Brain"); webView.destroy(); webView = null; }
        super.onDestroy();
    }
}
