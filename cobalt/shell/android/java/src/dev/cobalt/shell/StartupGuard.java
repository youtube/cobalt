package dev.cobalt.shell;

import static dev.cobalt.shell.Shell.TAG;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

/**
 * This class crashes the application if scheduled and not disarmed before its timer expires.
 *
 * <p>This can be used to mitigate applications being stuck and not able to make progress.
 *
 * <p>Intentionally crashing allows the system to capture a stack trace and potentially restart the
 * application, rather than leaving the user stuck on an unresponsive black screen.
 *
 * <p>StartupGuard also serves as the persistence bridge for the Chromium UMA funnel. Before the C++
 * JNI library (`libcobalt.so`) is fully unpacked and initialized, early startup milestones (1-4)
 * are recorded here. The bitmask is written to disk via a bare-metal `MappedByteBuffer` (bypassing
 * the heap) so it easily survives watchdog crashes. The C++ `ShellBrowserMainParts` harvester
 * collects this file on the subsequent boot and logs the rescued events to
 * `Cobalt.Startup.MilestoneReached`.
 */
public class StartupGuard {
  private final Handler handler;
  private final Runnable crashRunnable;
  private final AtomicLong startupStatus = new AtomicLong(0L);
  private final Map<String, String> diagnosisInfo = new HashMap<>();
  private final AtomicBoolean isArmed = new AtomicBoolean(false);

  private static class LazyHolder {
    private static final StartupGuard INSTANCE = new StartupGuard();
  }

  // Backing memory-mapped file for Phase 1 cross-layer UMA persistence
  private java.nio.MappedByteBuffer startupStateBuffer = null;

  // Private constructor prevents direct instantiation from other classes
  private StartupGuard() {
    // We attach the handler to the Main Looper to ensure the crash occurs on the UI thread
    handler = new Handler(Looper.getMainLooper());

    crashRunnable =
        new Runnable() {
          @Override
          public void run() {
            isArmed.set(false);
            throw new RuntimeException(
                "Application startup may not have succeeded, crash triggered by StartupGuard. "
                    + getStartupStatusAndDiagnosisInfo());
          }
        };
  }

  public void initializePersistence(android.content.Context context) {
    try {
      java.io.File file = new java.io.File(context.getFilesDir(), "java_startup_state.bin");
      // If a file from the previous session exists, rename it so C++ can harvest the previous
      // session's
      // state without racing with this fresh session's writes.
      if (file.exists()) {
        java.io.File prevFile =
            new java.io.File(context.getFilesDir(), "java_startup_state_previous.bin");
        file.renameTo(prevFile);
      }
      java.io.RandomAccessFile raf = new java.io.RandomAccessFile(file, "rw");
      // MappedByteBuffer defaults to big-endian, we strictly need little-endian for C++.
      startupStateBuffer =
          raf.getChannel().map(java.nio.channels.FileChannel.MapMode.READ_WRITE, 0, 8);
      startupStateBuffer.order(java.nio.ByteOrder.LITTLE_ENDIAN);
      startupStateBuffer.putLong(0, 0);
    } catch (Exception e) {
      Log.e(TAG, "Failed to map startup state file: " + e.getMessage());
    }
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
    long current = startupStatus.updateAndGet(curr -> curr | mask);

    if (startupStateBuffer != null) {
      startupStateBuffer.putLong(0, current);
    }
  }

  /**
   * Sets startup diagnosis info.
   *
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
   *
   * @param delaySeconds The delay in seconds before the crash is triggered.
   */
  public void scheduleCrash(long delaySeconds) {
    if (isArmed.compareAndSet(/* expect= */ false, /* update= */ true)) {
      handler.postDelayed(crashRunnable, delaySeconds * 1000);
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
    if (isArmed.compareAndSet(/* expect= */ true, /* update= */ false)) {
      handler.removeCallbacks(crashRunnable);
      Log.i(TAG, "StartupGuard cancelled crash. " + getStartupStatusAndDiagnosisInfo());
    }
  }

  /** Checks if the forced crash is currently scheduled. */
  @VisibleForTesting
  public boolean isArmed() {
    return isArmed.get();
  }

  /** Returns the runnable that triggers the forced crash. */
  @VisibleForTesting
  public Runnable getCrashRunnable() {
    return crashRunnable;
  }
}
