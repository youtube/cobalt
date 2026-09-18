package dev.cobalt.shell;

import static dev.cobalt.shell.Shell.TAG;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import org.chromium.base.metrics.RecordHistogram;

/**
 * This class crashes the application if scheduled and not disarmed before its timer expires.
 *
 * <p>This can be used to mitigate applications being stuck and not able to make progress.
 *
 * <p>Intentionally crashing allows the system to capture a stack trace and potentially restart the
 * application, rather than leaving the user stuck on an unresponsive black screen.
 */
public class StartupGuard {
  public static final String METRIC_MILESTONE_REACHED = "Cobalt.Startup.MilestoneReached";
  public static final String METRIC_MILESTONE_DURATION_PREFIX = "Cobalt.Startup.MilestoneDuration.";
  public static final String METRIC_MILESTONE_DURATION = "Cobalt.Startup.MilestoneDuration";
  public static final int MIN_LOGGED_MILESTONE = 5;
  public static final int MAX_LOGGED_MILESTONE = 37;

  private final Handler mHandler;
  private final Runnable mCrashRunnable;
  private final AtomicLong mStartupStatus = new AtomicLong(0L);
  private final Map<String, String> mDiagnosisInfo = new HashMap<>();
  private final AtomicBoolean mIsArmed = new AtomicBoolean(false);
  private final AtomicLong mLastMilestoneTimestampMs =
      new AtomicLong(SystemClock.elapsedRealtime());

  private static class LazyHolder {
    private static final StartupGuard INSTANCE = new StartupGuard();
  }

  // Private constructor prevents direct instantiation from other classes
  private StartupGuard() {
    // We attach the handler to the Main Looper to ensure the crash occurs on the UI thread
    mHandler = new Handler(Looper.getMainLooper());

    mCrashRunnable =
        new Runnable() {
          @Override
          public void run() {
            mIsArmed.set(false);
            throw new RuntimeException(
                "Application startup may not have succeeded, crash triggered by StartupGuard. "
                    + getStartupStatusAndDiagnosisInfo());
          }
        };
  }

  private String getStartupStatusAndDiagnosisInfo() {
    StringBuilder message = new StringBuilder();
    message.append("Status: 0x");
    message.append(Long.toHexString(mStartupStatus.get()));
    synchronized (mDiagnosisInfo) {
      if (!mDiagnosisInfo.isEmpty()) {
        message.append(", Diagnosis Info: ");
        message.append(mDiagnosisInfo.toString());
      }
    }
    return message.toString();
  }

  /**
   * Returns the single instance of StartupGuard. Uses the Initialization-on-demand holder idiom for
   * thread-safe lazy loading.
   */
  public static StartupGuard getInstance() {
    return LazyHolder.INSTANCE;
  }

  /**
   * Sets a milestone bit in the startup status.
   *
   * @param milestone The milestone to set, 0-indexed (0-63).
   */
  public void setStartupMilestone(int milestone) {
    if (milestone < 0 || milestone >= 64) {
      Log.e(TAG, "Invalid milestone: " + milestone);
      return;
    }
    Log.v(TAG, "StartupGuard setStartupMilestone:" + milestone);
    long mask = 1L << milestone;
    long previous = mStartupStatus.getAndUpdate(current -> current | mask);
    if ((previous & mask) != 0) {
      return;
    }

    if (milestone >= MIN_LOGGED_MILESTONE && milestone <= MAX_LOGGED_MILESTONE) {
      long now = SystemClock.elapsedRealtime();
      long previousTime = mLastMilestoneTimestampMs.getAndSet(now);
      if (previousTime > 0) {
        long durationMs = Math.max(0, now - previousTime);
        RecordHistogram.recordTimesHistogram(
            METRIC_MILESTONE_DURATION_PREFIX + milestone, durationMs);
        RecordHistogram.recordTimesHistogram(METRIC_MILESTONE_DURATION, durationMs);
      }
      RecordHistogram.recordEnumeratedHistogram(
          METRIC_MILESTONE_REACHED, milestone, MAX_LOGGED_MILESTONE + 1);
    }
  }

  /**
   * Sets startup diagnosis info.
   *
   * @param key The key for the diagnosis info.
   * @param value The value for the diagnosis info.
   */
  public void setDiagnosisInfo(String key, String value) {
    synchronized (mDiagnosisInfo) {
      Log.v(TAG, "StartupGuard setDiagnosisInfo: " + key + "=" + value);
      mDiagnosisInfo.put(key, value);
    }
  }

  /**
   * Schedules the forced crash to happen after the specified delay.
   *
   * @param delaySeconds The delay in seconds before the crash is triggered.
   */
  public void scheduleCrash(long delaySeconds) {
    if (mIsArmed.compareAndSet(/* expect= */ false, /* update= */ true)) {
      mHandler.postDelayed(mCrashRunnable, delaySeconds * 1000);
      Log.i(TAG, "StartupGuard scheduled crash in " + delaySeconds + " seconds.");
    } else {
      Log.w(
          TAG,
          "StartupGuard fail to schedule crash, because there is already a pending crash"
              + " scheduled.");
    }
  }

  /** Cancels the pending crash job. */
  public void disarm() {
    if (mIsArmed.compareAndSet(/* expect= */ true, /* update= */ false)) {
      mHandler.removeCallbacks(mCrashRunnable);
      Log.i(TAG, "StartupGuard cancelled crash. " + getStartupStatusAndDiagnosisInfo());
    }
  }

  /** Checks if the forced crash is currently scheduled. */
  @VisibleForTesting
  public boolean isArmed() {
    return mIsArmed.get();
  }

  /** Returns the runnable that triggers the forced crash. */
  @VisibleForTesting
  public Runnable getCrashRunnable() {
    return mCrashRunnable;
  }

  /** Resets internal state for testing. */
  @VisibleForTesting
  public void resetForTesting() {
    disarm();
    mStartupStatus.set(0L);
    synchronized (mDiagnosisInfo) {
      mDiagnosisInfo.clear();
    }
    mLastMilestoneTimestampMs.set(SystemClock.elapsedRealtime());
  }
}
