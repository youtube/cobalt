package dev.cobalt.shell;

import static dev.cobalt.shell.Shell.TAG;

import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicLongArray;
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
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({
    JAVA_ACTIVITY_CREATED,
    PRE_LIBRARY_LOADER_INIT,
    POST_LIBRARY_LOADER_INIT,
    PRE_STARBOARD_BRIDGE_INIT,
    STARBOARD_BRIDGE_NATIVE_INIT,
    APPLICATION_ANDROID_INITIALIZE,
    POST_STARBOARD_BRIDGE_INIT,
    ACTIVITY_WINDOW_CONFIGURED,
    ACTIVITY_ON_START,
    ACTIVITY_ON_RESUME,
    ACTIVITY_ON_PAUSE,
    ACTIVITY_ON_STOP,
    ACTIVITY_ON_DESTROY,
    MAIN_DELEGATE_BASIC_STARTUP_COMPLETE,
    MAIN_DELEGATE_PRE_SANDBOX_STARTUP,
    MAIN_DELEGATE_POST_EARLY_INITIALIZATION,
    PRE_CREATE_THREADS,
    SHELL_CREATE_WINDOW_NO_SPLASH,
    SHELL_CREATE_WINDOW_WITH_SPLASH,
    SHELL_RENDER_FRAME_CREATED,
    SHELL_DID_START_LOADING,
    DID_START_NAVIGATION,
    SPLASH_OBSERVER_DID_START_NAVIGATION,
    SPLASH_OBSERVER_DID_START_LOADING,
    SPLASH_OBSERVER_DID_FAIL_LOAD,
    DID_FINISH_NAVIGATION,
    SHELL_DOCUMENT_ELEMENT_AVAILABLE,
    SPLASH_OBSERVER_DID_FINISH_NAVIGATION,
    SHELL_DID_START_NAVIGATION_NON_SPLASH,
    SHELL_DID_FINISH_NAVIGATION_NON_SPLASH,
    SHELL_DID_FINISH_LOAD,
    STARTUP_GUARD_OBSERVER_DID_START_LOADING,
    STARTUP_GUARD_OBSERVER_DID_START_NAVIGATION,
    STARTUP_GUARD_OBSERVER_DID_FAIL_LOAD,
    STARTUP_GUARD_OBSERVER_DID_FINISH_NAVIGATION,
    STARTUP_GUARD_OBSERVER_DID_REDIRECT_NAVIGATION,
    PLATFORM_ERROR_RAISED,
  })
  public @interface Milestone {}

  public static final int JAVA_ACTIVITY_CREATED = 1;
  public static final int PRE_LIBRARY_LOADER_INIT = 2;
  public static final int POST_LIBRARY_LOADER_INIT = 3;
  public static final int PRE_STARBOARD_BRIDGE_INIT = 4;
  public static final int STARBOARD_BRIDGE_NATIVE_INIT = 5;
  public static final int APPLICATION_ANDROID_INITIALIZE = 6;
  public static final int POST_STARBOARD_BRIDGE_INIT = 7;
  public static final int ACTIVITY_WINDOW_CONFIGURED = 8;
  public static final int ACTIVITY_ON_START = 9;
  public static final int ACTIVITY_ON_RESUME = 10;
  public static final int ACTIVITY_ON_PAUSE = 11;
  public static final int ACTIVITY_ON_STOP = 12;
  public static final int ACTIVITY_ON_DESTROY = 13;
  public static final int MAIN_DELEGATE_BASIC_STARTUP_COMPLETE = 14;
  public static final int MAIN_DELEGATE_PRE_SANDBOX_STARTUP = 15;
  public static final int MAIN_DELEGATE_POST_EARLY_INITIALIZATION = 16;
  public static final int PRE_CREATE_THREADS = 17;
  public static final int SHELL_CREATE_WINDOW_NO_SPLASH = 18;
  public static final int SHELL_CREATE_WINDOW_WITH_SPLASH = 19;
  public static final int SHELL_RENDER_FRAME_CREATED = 20;
  public static final int SHELL_DID_START_LOADING = 21;
  public static final int DID_START_NAVIGATION = 22;
  public static final int SPLASH_OBSERVER_DID_START_NAVIGATION = 23;
  public static final int SPLASH_OBSERVER_DID_START_LOADING = 24;
  public static final int SPLASH_OBSERVER_DID_FAIL_LOAD = 25;
  public static final int DID_FINISH_NAVIGATION = 26;
  public static final int SHELL_DOCUMENT_ELEMENT_AVAILABLE = 27;
  public static final int SPLASH_OBSERVER_DID_FINISH_NAVIGATION = 28;
  public static final int SHELL_DID_START_NAVIGATION_NON_SPLASH = 29;
  public static final int SHELL_DID_FINISH_NAVIGATION_NON_SPLASH = 30;
  public static final int SHELL_DID_FINISH_LOAD = 31;
  public static final int STARTUP_GUARD_OBSERVER_DID_START_LOADING = 32;
  public static final int STARTUP_GUARD_OBSERVER_DID_START_NAVIGATION = 33;
  public static final int STARTUP_GUARD_OBSERVER_DID_FAIL_LOAD = 34;
  public static final int STARTUP_GUARD_OBSERVER_DID_FINISH_NAVIGATION = 35;
  public static final int STARTUP_GUARD_OBSERVER_DID_REDIRECT_NAVIGATION = 36;
  public static final int PLATFORM_ERROR_RAISED = 37;

  public static final String METRIC_MILESTONE_REACHED = "Cobalt.Startup.MilestoneReached";
  public static final String METRIC_TIME_TO_MILESTONE_PREFIX = "Cobalt.Startup.TimeToMilestone.";
  // Bucket layout for Cobalt.Startup.TimeToMilestone.*: 1 ms to 3 minutes in 100 buckets. The
  // default StartupGuard timeout is 120 s and a network dialog can legitimately keep startup going
  // beyond that; anything slower lands in the overflow bucket.
  private static final long TIME_TO_MILESTONE_MIN_MS = 1;
  private static final long TIME_TO_MILESTONE_MAX_MS = TimeUnit.MINUTES.toMillis(3);
  private static final int TIME_TO_MILESTONE_BUCKETS = 100;
  public static final int MIN_LOGGED_MILESTONE = 1;
  public static final String[] MILESTONE_NAMES = {
    "", // 0 (unused)
    "JavaActivityCreated", // 1
    "PreLibraryLoaderInit", // 2
    "PostLibraryLoaderInit", // 3
    "PreStarboardBridgeInit", // 4
    "StarboardBridgeNativeInit", // 5
    "ApplicationAndroidInitialize", // 6
    "PostStarboardBridgeInit", // 7
    "ActivityWindowConfigured", // 8
    "ActivityOnStart", // 9
    "ActivityOnResume", // 10
    "ActivityOnPause", // 11
    "ActivityOnStop", // 12
    "ActivityOnDestroy", // 13
    "MainDelegateBasicStartupComplete", // 14
    "MainDelegatePreSandboxStartup", // 15
    "MainDelegatePostEarlyInitialization", // 16
    "PreCreateThreads", // 17
    "ShellCreateWindowNoSplash", // 18
    "ShellCreateWindowWithSplash", // 19
    "ShellRenderFrameCreated", // 20
    "ShellDidStartLoading", // 21
    "DidStartNavigation", // 22
    "SplashObserverDidStartNavigation", // 23
    "SplashObserverDidStartLoading", // 24
    "SplashObserverDidFailLoad", // 25
    "DidFinishNavigation", // 26
    "ShellDocumentElementAvailable", // 27
    "SplashObserverDidFinishNavigation", // 28
    "ShellDidStartNavigationNonSplash", // 29
    "ShellDidFinishNavigationNonSplash", // 30
    "ShellDidFinishLoad", // 31
    "StartupGuardObserverDidStartLoading", // 32
    "StartupGuardObserverDidStartNavigation", // 33
    "StartupGuardObserverDidFailLoad", // 34
    "StartupGuardObserverDidFinishNavigation", // 35
    "StartupGuardObserverDidRedirectNavigation", // 36
    "PlatformErrorRaised", // 37
  };
  public static final int MAX_LOGGED_MILESTONE = MILESTONE_NAMES.length - 1;

  private final Handler mHandler;
  private final Runnable mCrashRunnable;
  private final AtomicLong mStartupStatus = new AtomicLong(0L);
  private final Map<String, String> mDiagnosisInfo = new HashMap<>();
  private final AtomicBoolean mIsArmed = new AtomicBoolean(false);
  // Origin for Cobalt.Startup.TimeToMilestone.*, on the SystemClock.elapsedRealtime() clock.
  private volatile long mStartupOriginMs = getProcessStartElapsedRealtime();
  // SystemClock.elapsedRealtime() at which each milestone was first reached; 0 if not reached.
  private final AtomicLongArray mMilestoneTimestampsMs = new AtomicLongArray(64);
  // Time-to-milestone samples describe a cold start only while the process is still in its first
  // Activity. Cleared once that Activity is destroyed or a second Activity is created in this
  // process, after which milestones reached for the first time are no longer startup events.
  private final AtomicBoolean mIsStartupTimingActive = new AtomicBoolean(true);
  private volatile Runnable mPreCrashHook;

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
            Runnable hook = mPreCrashHook;
            if (hook != null) {
              try {
                hook.run();
              } catch (Throwable t) {
                Log.e(TAG, "Error running preCrashHook in StartupGuard", t);
              }
            }
            throw new RuntimeException(
                "Application startup may not have succeeded, crash triggered by StartupGuard. "
                    + getStartupStatusAndDiagnosisInfo());
          }
        };
  }

  /**
   * Sets a hook to be executed immediately before StartupGuard triggers a forced crash.
   *
   * @param hook The Runnable to execute prior to throwing the runtime exception.
   */
  public void setPreCrashHook(Runnable hook) {
    this.mPreCrashHook = hook;
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
   * <p>The first time a logged milestone is reached, Cobalt.Startup.MilestoneReached records the
   * milestone and Cobalt.Startup.TimeToMilestone.{Milestone} records the time since the process
   * started. Every milestone shares that origin, so the samples are comparable across milestones
   * regardless of the order in which the concurrent startup sequences reach them.
   *
   * @param milestone The milestone to set, 0-indexed (0-63).
   */
  public void setStartupMilestone(@Milestone int milestone) {
    if (milestone < 0 || milestone >= 64) {
      Log.e(TAG, "Invalid milestone: " + milestone);
      return;
    }
    Log.v(TAG, "StartupGuard setStartupMilestone:" + milestone);
    long mask = 1L << milestone;
    long previous = mStartupStatus.getAndUpdate(current -> current | mask);
    if ((previous & mask) != 0) {
      if (milestone == JAVA_ACTIVITY_CREATED) {
        // A second Activity in this process: this is no longer the cold start measured from the
        // process start, so stop timing milestones that are only now being reached.
        mIsStartupTimingActive.set(false);
      }
      return;
    }

    long now = SystemClock.elapsedRealtime();
    mMilestoneTimestampsMs.set(milestone, now);
    if (milestone >= MIN_LOGGED_MILESTONE && milestone <= MAX_LOGGED_MILESTONE) {
      if (mIsStartupTimingActive.get()) {
        RecordHistogram.recordCustomTimesHistogram(
            METRIC_TIME_TO_MILESTONE_PREFIX + MILESTONE_NAMES[milestone],
            Math.max(0, now - mStartupOriginMs),
            TIME_TO_MILESTONE_MIN_MS,
            TIME_TO_MILESTONE_MAX_MS,
            TIME_TO_MILESTONE_BUCKETS);
      }
      RecordHistogram.recordSparseHistogram(METRIC_MILESTONE_REACHED, milestone);
    }
    if (milestone == ACTIVITY_ON_DESTROY) {
      mIsStartupTimingActive.set(false);
    }
  }

  /**
   * Returns the process start time on the SystemClock.elapsedRealtime() clock, which predates the
   * Activity's onCreate (zygote fork, Application creation, class loading). Falls back to the
   * current time, i.e. the StartupGuard's creation during Activity creation, where the platform did
   * not stamp the process start (e.g. under Robolectric).
   */
  private static long getProcessStartElapsedRealtime() {
    long processStartMs = Process.getStartElapsedRealtime();
    return processStartMs > 0 ? processStartMs : SystemClock.elapsedRealtime();
  }

  /** Returns the bitmask of milestones reached during startup. */
  public long getStartupStatus() {
    return mStartupStatus.get();
  }

  /**
   * Returns the SystemClock.elapsedRealtime() at which {@code milestone} was first reached, or 0 if
   * it has not been reached.
   */
  public long getMilestoneTimestampMs(@Milestone int milestone) {
    if (milestone < 0 || milestone >= 64) {
      return 0;
    }
    return mMilestoneTimestampsMs.get(milestone);
  }

  /** Returns the highest milestone reached during startup (0 if none reached). */
  public int getHighestMilestone() {
    long status = mStartupStatus.get();
    if (status == 0L) {
      return 0;
    }
    return 63 - Long.numberOfLeadingZeros(status);
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

  /** Overrides the Cobalt.Startup.TimeToMilestone.* origin (SystemClock.elapsedRealtime() ms). */
  @VisibleForTesting
  public void setStartupOriginForTesting(long startupOriginMs) {
    mStartupOriginMs = startupOriginMs;
  }

  /** Resets internal state for testing. */
  @VisibleForTesting
  public void resetForTesting() {
    disarm();
    mStartupStatus.set(0L);
    synchronized (mDiagnosisInfo) {
      mDiagnosisInfo.clear();
    }
    mStartupOriginMs = getProcessStartElapsedRealtime();
    for (int i = 0; i < mMilestoneTimestampsMs.length(); i++) {
      mMilestoneTimestampsMs.set(i, 0L);
    }
    mIsStartupTimingActive.set(true);
    mPreCrashHook = null;
  }
}
