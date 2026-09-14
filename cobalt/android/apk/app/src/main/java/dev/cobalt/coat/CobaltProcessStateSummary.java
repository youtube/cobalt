// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import org.chromium.base.library_loader.LibraryLoader;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Manages attaching compact process state summaries to the Android OS via
 * ActivityManager.setProcessStateSummary() (API 30+) during session lifetime, and recovering them
 * on subsequent startup via getHistoricalProcessExitReasons().
 *
 * <p>Lifetime and ownership: Stateless utility class that cannot be instantiated. It has an
 * infinite lifetime and does not require instance ownership.
 *
 * <p>Threading model: Thread-safe. All public static methods can be called concurrently from any
 * thread (e.g., UI thread, background worker threads, or during early startup / crash handlers).
 */
@JNINamespace("cobalt")
public class CobaltProcessStateSummary {
  private static final String TAG = "CobaltProcessSummary";
  private static final int MAX_SUMMARY_BYTES = 128;

  /** Sentinel value indicating that querying ApplicationExitInfo failed or returned empty. */
  public static final int SYSTEM_REASON_API_FAILED = -1;

  private CobaltProcessStateSummary() {}

  /** Standard exit reason codes matching Chromium ProcessExitReasonFromSystem UMA enum values. */
  @IntDef({
    ExitReason.REASON_ANR,
    ExitReason.REASON_CRASH,
    ExitReason.REASON_CRASH_NATIVE,
    ExitReason.REASON_DEPENDENCY_DIED,
    ExitReason.REASON_EXCESSIVE_RESOURCE_USAGE,
    ExitReason.REASON_EXIT_SELF,
    ExitReason.REASON_INITIALIZATION_FAILURE,
    ExitReason.REASON_LOW_MEMORY,
    ExitReason.REASON_OTHER,
    ExitReason.REASON_PERMISSION_CHANGE,
    ExitReason.REASON_SIGNALED,
    ExitReason.REASON_UNKNOWN,
    ExitReason.REASON_USER_REQUESTED,
    ExitReason.REASON_USER_STOPPED,
    ExitReason.REASON_API_FAILED,
    ExitReason.REASON_FREEZER,
    ExitReason.REASON_PACKAGE_STATE_CHANGE,
    ExitReason.REASON_PACKAGE_UPDATED,
  })
  @Retention(RetentionPolicy.SOURCE)
  public @interface ExitReason {
    int REASON_ANR = 0;
    int REASON_CRASH = 1;
    int REASON_CRASH_NATIVE = 2;
    int REASON_DEPENDENCY_DIED = 3;
    int REASON_EXCESSIVE_RESOURCE_USAGE = 4;
    int REASON_EXIT_SELF = 5;
    int REASON_INITIALIZATION_FAILURE = 6;
    int REASON_LOW_MEMORY = 7;
    int REASON_OTHER = 8;
    int REASON_PERMISSION_CHANGE = 9;
    int REASON_SIGNALED = 10;
    int REASON_UNKNOWN = 11;
    int REASON_USER_REQUESTED = 12;
    int REASON_USER_STOPPED = 13;
    int REASON_API_FAILED = 14;
    int REASON_FREEZER = 15;
    int REASON_PACKAGE_STATE_CHANGE = 16;
    int REASON_PACKAGE_UPDATED = 17;
    int NUM_ENTRIES = 18;
  }

  private static ActivityManager sActivityManagerForTesting;

  private static ActivityManager getActivityManager() {
    if (sActivityManagerForTesting != null) {
      return sActivityManagerForTesting;
    }
    BaseStarboardBridge bridge = BaseStarboardBridge.getInstance();
    if (bridge == null) {
      return null;
    }
    Context context = bridge.getApplicationContext();
    if (context == null) {
      return null;
    }
    return (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
  }

  @VisibleForTesting
  public static void setActivityManagerForTesting(ActivityManager am) {
    sActivityManagerForTesting = am;
  }

  @VisibleForTesting
  public static void resetForTesting() {
    sActivityManagerForTesting = null;
    sLastSummaryBytes = null;
  }

  /**
   * Helper class to isolate Android R (API 30+) specific ActivityManager and ApplicationExitInfo
   * calls, preventing class verification failures on older Android releases.
   */
  @RequiresApi(Build.VERSION_CODES.R)
  public static final class ApiHelperForR {
    private ApiHelperForR() {}

    public static void setProcessStateSummary(ActivityManager am, byte[] summaryBytes) {
      am.setProcessStateSummary(summaryBytes);
    }

    public static @Nullable ApplicationExitInfo getLatestApplicationExitInfo(ActivityManager am) {
      try {
        List<ApplicationExitInfo> reasons =
            am.getHistoricalProcessExitReasons(
                /* packageName= */ null, /* pid= */ 0, /* maxNum= */ 1);
        if (reasons == null || reasons.isEmpty() || reasons.get(0) == null) {
          return null;
        }
        return reasons.get(0);
      } catch (RuntimeException e) {
        Log.w(TAG, "Failed to get historical process exit reasons", e);
        return null;
      }
    }

    public static @Nullable byte[] getProcessStateSummary(ApplicationExitInfo info) {
      return info.getProcessStateSummary();
    }

    public static int getReason(ApplicationExitInfo info) {
      return info.getReason();
    }

    public static @Nullable Integer convertToExitReason(int systemReason) {
      switch (systemReason) {
        case SYSTEM_REASON_API_FAILED:
          return ExitReason.REASON_API_FAILED;
        case ApplicationExitInfo.REASON_ANR:
          return ExitReason.REASON_ANR;
        case ApplicationExitInfo.REASON_CRASH:
          return ExitReason.REASON_CRASH;
        case ApplicationExitInfo.REASON_CRASH_NATIVE:
          return ExitReason.REASON_CRASH_NATIVE;
        case ApplicationExitInfo.REASON_DEPENDENCY_DIED:
          return ExitReason.REASON_DEPENDENCY_DIED;
        case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE:
          return ExitReason.REASON_EXCESSIVE_RESOURCE_USAGE;
        case ApplicationExitInfo.REASON_EXIT_SELF:
          return ExitReason.REASON_EXIT_SELF;
        case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE:
          return ExitReason.REASON_INITIALIZATION_FAILURE;
        case ApplicationExitInfo.REASON_LOW_MEMORY:
          return ExitReason.REASON_LOW_MEMORY;
        case ApplicationExitInfo.REASON_OTHER:
          return ExitReason.REASON_OTHER;
        case ApplicationExitInfo.REASON_PERMISSION_CHANGE:
          return ExitReason.REASON_PERMISSION_CHANGE;
        case ApplicationExitInfo.REASON_SIGNALED:
          return ExitReason.REASON_SIGNALED;
        case ApplicationExitInfo.REASON_UNKNOWN:
          return ExitReason.REASON_UNKNOWN;
        case ApplicationExitInfo.REASON_USER_REQUESTED:
          return ExitReason.REASON_USER_REQUESTED;
        case ApplicationExitInfo.REASON_USER_STOPPED:
          return ExitReason.REASON_USER_STOPPED;
        case ApplicationExitInfo.REASON_FREEZER:
          return ExitReason.REASON_FREEZER;
        case ApplicationExitInfo.REASON_PACKAGE_STATE_CHANGE:
          return ExitReason.REASON_PACKAGE_STATE_CHANGE;
        case ApplicationExitInfo.REASON_PACKAGE_UPDATED:
          return ExitReason.REASON_PACKAGE_UPDATED;
        default:
          return ExitReason.REASON_OTHER;
      }
    }
  }

  /** Converts Android system ApplicationExitInfo REASON_* constants to standard exit reasons. */
  public static @Nullable Integer convertToExitReason(int systemReason) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return null;
    }
    return ApiHelperForR.convertToExitReason(systemReason);
  }

  private static final byte MAGIC_BYTE = (byte) 0xCB;
  private static final byte VERSION_BYTE = 2;
  private static final int CHECKSUM_DATA_LENGTH = 36;
  private static final int TOTAL_PAYLOAD_LENGTH = 40;
  private static final int FLAGS_BYTE_INDEX = 3;
  private static final byte FLAG_STARTUP_GUARD_TRIGGERED_KILL = 1 << 3;
  private static volatile byte[] sLastSummaryBytes;

  private static int get16bits(byte[] data, int offset) {
    return ((data[offset + 1] & 0xFF) << 8) | (data[offset] & 0xFF);
  }

  /**
   * Pure Java implementation of SuperFastHash (base::PersistentHash) used as a fallback when native
   * libraries are not yet loaded.
   */
  @VisibleForTesting
  public static int computePersistentHashJava(byte[] data, int length) {
    if (data == null || length <= 0) {
      return 0;
    }
    int hash = length;
    int rem = length & 3;
    int blocks = length >> 2;
    int offset = 0;

    for (int i = 0; i < blocks; ++i) {
      hash += get16bits(data, offset);
      int tmp = (get16bits(data, offset + 2) << 11) ^ hash;
      hash = (hash << 16) ^ tmp;
      offset += 4;
      hash += hash >>> 11;
    }

    switch (rem) {
      case 3:
        hash += get16bits(data, offset);
        hash ^= hash << 16;
        hash ^= ((int) data[offset + 2]) << 18;
        hash += hash >>> 11;
        break;
      case 2:
        hash += get16bits(data, offset);
        hash ^= hash << 11;
        hash += hash >>> 17;
        break;
      case 1:
        hash += (int) data[offset];
        hash ^= hash << 10;
        hash += hash >>> 1;
        break;
      default:
        break;
    }

    hash ^= hash << 3;
    hash += hash >>> 5;
    hash ^= hash << 4;
    hash += hash >>> 17;
    hash ^= hash << 25;
    hash += hash >>> 6;

    return hash;
  }

  /**
   * Computes a 32-bit PersistentHash (SuperFastHash) matching Chromium's base::PersistentHash
   * implementation over the first |length| bytes of |data|.
   */
  @VisibleForTesting
  public static int computePersistentHash(byte[] data, int length) {
    if (data == null || length <= 0) {
      return 0;
    }
    if (LibraryLoader.getInstance().isInitialized()) {
      try {
        return CobaltProcessStateSummaryJni.get().computePersistentHash(data, length);
      } catch (UnsatisfiedLinkError e) {
        Log.w(TAG, "Native library reported initialized but JNI method missing", e);
      }
    }
    return computePersistentHashJava(data, length);
  }

  @CalledByNative
  public static void setProcessStateSummary(byte[] summaryBytes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    if (summaryBytes != null && summaryBytes.length > MAX_SUMMARY_BYTES) {
      Log.w(TAG, "Process state summary exceeds 128 bytes limit.");
      return;
    }
    sLastSummaryBytes = summaryBytes != null ? summaryBytes.clone() : null;
    try {
      ActivityManager am = getActivityManager();
      if (am != null) {
        ApiHelperForR.setProcessStateSummary(am, summaryBytes);
      }
    } catch (Exception e) {
      Log.e(TAG, "Failed to setProcessStateSummary: ", e);
    }
  }

  /**
   * Sets the kFlagStartupGuardTriggeredKill flag in the active process state summary immediately
   * before StartupGuard forces a process crash, updating the 32-bit checksum.
   */
  public static void setStartupGuardTriggeredKill() {
    setStartupGuardTriggeredKill(0L, 0);
  }

  /**
   * Sets the kFlagStartupGuardTriggeredKill flag in the active process state summary immediately
   * before StartupGuard forces a process crash, embedding the provided milestone data and updating
   * the 32-bit checksum.
   *
   * @param startupStatus The 64-bit mask of startup milestones reached.
   * @param highestMilestone The highest milestone reached during startup.
   */
  public static void setStartupGuardTriggeredKill(long startupStatus, int highestMilestone) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return;
      }
      byte[] bytes = sLastSummaryBytes;
      if (bytes != null && bytes.length >= TOTAL_PAYLOAD_LENGTH) {
        bytes = bytes.clone();
        bytes[FLAGS_BYTE_INDEX] =
            (byte) (bytes[FLAGS_BYTE_INDEX] | FLAG_STARTUP_GUARD_TRIGGERED_KILL);
      } else {
        bytes = new byte[TOTAL_PAYLOAD_LENGTH];
        bytes[0] = MAGIC_BYTE;
        bytes[1] = VERSION_BYTE;
        bytes[FLAGS_BYTE_INDEX] = FLAG_STARTUP_GUARD_TRIGGERED_KILL;
        int pid = android.os.Process.myPid();
        bytes[4] = (byte) (pid & 0xFF);
        bytes[5] = (byte) ((pid >> 8) & 0xFF);
        bytes[6] = (byte) ((pid >> 16) & 0xFF);
        bytes[7] = (byte) ((pid >> 24) & 0xFF);
      }

      // Populate bytes [24..32] with startup milestones mask and highest milestone.
      for (int i = 0; i < 8; ++i) {
        bytes[24 + i] = (byte) ((startupStatus >>> (i * 8)) & 0xFF);
      }
      bytes[32] = (byte) (highestMilestone & 0xFF);

      // Recompute 32-bit checksum over data bytes [0..35] and write to [36..39].
      int checksum = computePersistentHash(bytes, CHECKSUM_DATA_LENGTH);
      bytes[36] = (byte) (checksum & 0xFF);
      bytes[37] = (byte) ((checksum >> 8) & 0xFF);
      bytes[38] = (byte) ((checksum >> 16) & 0xFF);
      bytes[39] = (byte) ((checksum >> 24) & 0xFF);

      sLastSummaryBytes = bytes;
      ApiHelperForR.setProcessStateSummary(am, bytes);
    } catch (Exception e) {
      Log.e(TAG, "Failed to setStartupGuardTriggeredKill: ", e);
    }
  }

  @CalledByNative
  public static @Nullable byte[] getPriorSessionProcessStateSummary() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return null;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return null;
      }
      ApplicationExitInfo info = ApiHelperForR.getLatestApplicationExitInfo(am);
      return info != null ? ApiHelperForR.getProcessStateSummary(info) : null;
    } catch (Exception e) {
      Log.e(TAG, "Failed to getHistoricalProcessExitReasons summary: ", e);
      return null;
    }
  }

  @CalledByNative
  public static int recordLatestExitReasonToUma(String umaName) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return -1;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return -1;
      }
      ApplicationExitInfo info = ApiHelperForR.getLatestApplicationExitInfo(am);
      if (info == null) {
        return -1;
      }
      int systemReason = ApiHelperForR.getReason(info);
      Integer exitReason = ApiHelperForR.convertToExitReason(systemReason);
      return exitReason != null ? exitReason : -1;
    } catch (Exception e) {
      Log.e(TAG, "Failed to recordLatestExitReasonToUma: ", e);
      return -1;
    }
  }

  /**
   * Consolidates querying the latest exit reason into a single Binder transaction: records the
   * system exit reason to outExitReason (if provided), and returns the prior session's process
   * state summary bytes.
   */
  public static @Nullable byte[] recordLatestExitReasonAndGetSummary(String umaName) {
    return recordLatestExitReasonAndGetSummary(umaName, null);
  }

  @CalledByNative
  public static @Nullable byte[] recordLatestExitReasonAndGetSummary(
      String umaName, @Nullable int[] outExitReason) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return null;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return null;
      }
      ApplicationExitInfo info = ApiHelperForR.getLatestApplicationExitInfo(am);
      if (info == null) {
        return null;
      }
      int systemReason = ApiHelperForR.getReason(info);
      Integer exitReason = ApiHelperForR.convertToExitReason(systemReason);
      if (exitReason != null) {
        if (outExitReason != null && outExitReason.length > 0) {
          outExitReason[0] = exitReason;
        }
      }
      return ApiHelperForR.getProcessStateSummary(info);
    } catch (Exception e) {
      Log.e(TAG, "Failed to recordLatestExitReasonAndGetSummary: ", e);
      return null;
    }
  }

  @NativeMethods
  interface Natives {
    int computePersistentHash(byte[] data, int length);
  }
}
