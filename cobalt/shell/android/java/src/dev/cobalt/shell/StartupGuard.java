package dev.cobalt.shell;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

/**
 * This class crashes the application if scheduled and not disarmed
 * before its timer expires.
 *
 * This can be used to mitigate applications being stuck and not
 * able to make progress.
 *
 * Intentionally crashing allows the system to capture a stack trace and potentially
 * restart the application, rather than leaving the user stuck on an unresponsive black screen.
 */
public class StartupGuard {
    private final Handler handler;
    private final Runnable crashRunnable;
    private final AtomicLong startupStatus = new AtomicLong(0L);
    private final Map<String, String> diagnosisInfo = new HashMap<>();

    private static class LazyHolder {
        private static final StartupGuard INSTANCE = new StartupGuard();
    }

    private static final String TAG = "cobalt";

    // Private constructor prevents direct instantiation from other classes
    private StartupGuard() {
        // We attach the handler to the Main Looper to ensure the crash occurs on the UI thread
        handler = new Handler(Looper.getMainLooper());

        crashRunnable = new Runnable() {
            @Override
            public void run() {
                throw new RuntimeException(
                        "Application startup may not have succeeded, crash triggered by StartupGuard. "
                                + getStartupStatusAndDiagnosisInfo());
            }
        };
    }

    private String getStartupStatusAndDiagnosisInfo() {
        StringBuilder message = new StringBuilder();
        message.append("Status: 0x");
        message.append(Long.toHexString(startupStatus.get()));
        synchronized (diagnosisInfo) {
            if (!diagnosisInfo.isEmpty()) {
                message.append(", Diagnosis Info: ");
                message.append(diagnosisInfo.toString());
            }
        }
        return message.toString();
    }

    /**
     * Returns the single instance of StartupGuard.
     * Uses the Initialization-on-demand holder idiom for thread-safe lazy loading.
     */
    public static StartupGuard getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Sets a milestone bit in the startup status.
     * @param milestone The milestone to set, 0-indexed (0-63).
     */
    public void setStartupMilestone(int milestone) {
        if (milestone < 0 || milestone >= 64) {
            Log.e(TAG, "Invalid milestone: " + milestone);
            return;
        }
        Log.v(TAG, "StartupGuard setStartupMilestone:" + milestone);
        long mask = 1L << milestone;
        startupStatus.updateAndGet(current -> current | mask);
    }

    /**
     * Sets startup diagnosis info.
     * @param key The key for the diagnosis info.
     * @param value The value for the diagnosis info.
     */
    public void setDiagnosisInfo(String key, String value) {
        synchronized (diagnosisInfo) {
            Log.v(TAG, "StartupGuard setDiagnosisInfo: " + key + "=" + value);
            diagnosisInfo.put(key, value);
        }
    }

    /**
     * Schedules the forced crash to happen after the specified delay.
     * @param delaySeconds The delay in seconds before the crash is triggered.
     */
    public void scheduleCrash(long delaySeconds) {
        if (!handler.hasCallbacks(crashRunnable)) {
            handler.postDelayed(crashRunnable, delaySeconds * 1000);
            Log.i(TAG, "StartupGuard scheduled crash in " + delaySeconds + " seconds.");
        } else {
            Log.w(TAG, "StartupGuard fail to schedule crash, because there is already a pending crash scheduled.");
        }
    }

    /**
     * Cancels the pending crash job.
     */
    public void disarm() {
        if (handler.hasCallbacks(crashRunnable)) {
            handler.removeCallbacks(crashRunnable);
            Log.i(TAG, "StartupGuard cancelled crash. " + getStartupStatusAndDiagnosisInfo());
        }
    }
}
