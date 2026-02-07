package dev.cobalt.shell;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

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
    private int startupStatus = 0;

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
                        "Application startup may not have succeeded, crash triggered by StartupGuard. Status: " + startupStatus);
            }
        };
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
     * @param milestone The milestone to set, 0-indexed.
     */
    public synchronized void setStartupMilestone(int milestone) {
        Log.i(TAG, "StartupGuard setStartupMilestone:" + milestone);
        startupStatus |= (1 << milestone);
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
            Log.i(TAG, "StartupGuard cancelled crash.");
            Log.i(TAG, "StartupGuard Status:" + startupStatus);
        }
    }
}
