package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

public class AppExitScheduler {
    private static volatile AppExitScheduler instance;

    private final Handler handler;
    private final Runnable crashRunnable;

    // Private constructor prevents direct instantiation from other classes
    private AppExitScheduler() {
        // We attach the handler to the Main Looper to ensure the crash occurs on the UI thread
        handler = new Handler(Looper.getMainLooper());
        
        crashRunnable = new Runnable() {
            @Override
            public void run() {
                throw new RuntimeException(
                        "The user may stuck on the black screen. Crash triggered by AppExitScheduler.");
            }
        };
    }

    /**
     * Returns the single instance of AppExitScheduler.
     * Uses double-checked locking for thread safety.
     */
    public static AppExitScheduler getInstance() {
        if (instance == null) {
            synchronized (AppExitScheduler.class) {
                if (instance == null) {
                    instance = new AppExitScheduler();
                }
            }
        }
        return instance;
    }

    /**
     * Schedules the forced crash to happen after the specified delay.
     * @param delaySeconds The delay in seconds before the crash is triggered.
     */
    public void scheduleCrash(long delaySeconds) {
        handler.postDelayed(crashRunnable, delaySeconds * 1000);
        Log.i(TAG, "AppExitScheduler scheduled crash in " + delaySeconds + " seconds.");
    }

    /**
     * Cancels the pending crash job.
     */
    public void cancelCrash() {
        if (handler.hasCallbacks(crashRunnable)) {
            Log.i(TAG, "AppExitScheduler cancelled crash.");
            handler.removeCallbacks(crashRunnable);
        }
    }
}