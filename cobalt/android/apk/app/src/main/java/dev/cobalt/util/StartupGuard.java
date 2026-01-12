package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * A scheduler designed to mitigate "black screen" issues during app startup.
 *
 * This class schedules a forced runtime exception if the Youtube app fails to
 * transition past the splash screen within a safety timeout.
 * Youtube calls window.h5vcc.system.hideSplashScreen() for this transition.
 *
 * Intentionally crashing allows the system to capture a stack trace and potentially
 * restart the application, rather than leaving the user stuck on an unresponsive black screen.
 */
public class StartupGuard {
    private final Handler handler;
    private final Runnable crashRunnable;

    private static class LazyHolder {
        private static final StartupGuard INSTANCE = new StartupGuard();
    }

    // Private constructor prevents direct instantiation from other classes
    private StartupGuard() {
        // We attach the handler to the Main Looper to ensure the crash occurs on the UI thread
        handler = new Handler(Looper.getMainLooper());

        crashRunnable = new Runnable() {
            @Override
            public void run() {
                throw new RuntimeException(
                        "Application startup may not have succeeded, crash triggered by StartupGuard.");
            }
        };
    }

    /**
     * Returns the single instance of StartupGuard.
     * Uses double-checked locking for thread safety.
     */
    public static StartupGuard getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Schedules the forced crash to happen after the specified delay.
     * @param delaySeconds The delay in seconds before the crash is triggered.
     */
    public void scheduleCrash(long delaySeconds) {
        handler.postDelayed(crashRunnable, delaySeconds * 1000);
        Log.i(TAG, "StartupGuard scheduled crash in " + delaySeconds + " seconds.");
    }

    /**
     * Cancels the pending crash job.
     */
    public void disarm() {
        if (handler.hasCallbacks(crashRunnable)) {
            handler.removeCallbacks(crashRunnable);
            Log.i(TAG, "StartupGuard cancelled crash.");
        }
    }
}
