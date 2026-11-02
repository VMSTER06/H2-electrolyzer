package by.h2pro.control;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** v1.2: после перезагрузки телефона снова запустить наблюдение, если оно было включено. */
public class H2BootReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        String a = intent.getAction();
        if ((Intent.ACTION_BOOT_COMPLETED.equals(a) || Intent.ACTION_MY_PACKAGE_REPLACED.equals(a))
                && H2MonitorService.isEnabled(context)) {
            try { H2MonitorService.start(context); } catch (Exception ignored) { }
        }
    }
}
