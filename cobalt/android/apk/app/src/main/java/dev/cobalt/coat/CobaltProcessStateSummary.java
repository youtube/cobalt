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

import static dev.cobalt.util.Log.TAG;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.util.List;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Java bridge for attaching and retrieving the 16-byte process state summary via Android R+ (API
 * 30+) ActivityManager APIs.
 */
@JNINamespace("cobalt")
public final class CobaltProcessStateSummary {

  public static final byte MAGIC_BYTE = (byte) 0xCB;
  public static final byte VERSION_BYTE = 0x01;
  public static final byte FLAG_STARTUP_GUARD_ARMED = 0x01;
  public static final byte FLAG_STARTUP_GUARD_TRIGGERED_KILL = 0x02;

  public static final int MAGIC_BYTE_INDEX = 0;
  public static final int VERSION_BYTE_INDEX = 1;
  public static final int FLAGS_BYTE_INDEX = 2;
  public static final int STARTUP_MILESTONES_BYTE_INDEX = 3;
  public static final int HIGHEST_MILESTONE_BYTE_INDEX = 11;
  public static final int CHECKSUM_BYTE_INDEX = 12;

  public static final int CHECKSUM_DATA_LENGTH = 12;
  public static final int TOTAL_PAYLOAD_LENGTH = 16;
  public static final int MAX_SUMMARY_BYTES = 128;

  private static ActivityManager sActivityManagerForTesting;
  private static volatile byte[] sLastSummaryBytes;

  @RequiresApi(Build.VERSION_CODES.R)
  public static final class ApiHelperForR {
    private ApiHelperForR() {}

    public static void setProcessStateSummary(ActivityManager am, byte[] summaryBytes) {
      try {
        am.setProcessStateSummary(summaryBytes);
      } catch (RuntimeException e) {
        Log.e(TAG, "Failed to call ActivityManager.setProcessStateSummary: ", e);
      }
    }

    public static @Nullable byte[] getProcessStateSummary(ApplicationExitInfo info) {
      try {
        return info.getProcessStateSummary();
      } catch (RuntimeException e) {
        Log.e(TAG, "Failed to get process state summary from ApplicationExitInfo: ", e);
        return null;
      }
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
        Log.w(TAG, "Failed to query historical process exit reasons: ", e);
        return null;
      }
    }

    public static int getReason(ApplicationExitInfo info) {
      return info.getReason();
    }

    public static @Nullable Integer convertToExitReason(int systemReason) {
      if (systemReason >= ApplicationExitInfo.REASON_EXIT_SELF
          && systemReason <= ApplicationExitInfo.REASON_PACKAGE_UPDATED) {
        return systemReason - 1;
      }
      return null;
    }
  }

  @VisibleForTesting
  public static void setActivityManagerForTesting(ActivityManager am) {
    sActivityManagerForTesting = am;
  }

  @VisibleForTesting
  public static void resetForTesting() {
    sLastSummaryBytes = null;
    sActivityManagerForTesting = null;
  }

  private static @Nullable ActivityManager getActivityManager() {
    if (sActivityManagerForTesting != null) {
      return sActivityManagerForTesting;
    }
    Context context = ContextUtils.getApplicationContext();
    if (context == null) {
      BaseStarboardBridge bridge = BaseStarboardBridge.getInstance();
      if (bridge != null) {
        context = bridge.getApplicationContext();
      }
    }
    return context != null
        ? (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE)
        : null;
  }

  private static int get16bits(byte[] data, int offset) {
    return ((data[offset + 1] & 0xFF) << 8) | (data[offset] & 0xFF);
  }

  /** Pure Java implementation of SuperFastHash (base::PersistentHash) fallback. */
  @VisibleForTesting
  public static int computePersistentHashJava(byte[] data, int length) {
    if (data == null || length <= 0) return 0;
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
    }

    hash ^= hash << 3;
    hash += hash >>> 5;
    hash ^= hash << 4;
    hash += hash >>> 17;
    hash ^= hash << 25;
    hash += hash >>> 6;
    return hash;
  }

  /** Computes a 32-bit PersistentHash (SuperFastHash) matching Chromium's base::PersistentHash. */
  @VisibleForTesting
  public static int computePersistentHash(byte[] data, int length) {
    if (data == null || length <= 0) return 0;
    if (LibraryLoader.getInstance().isInitialized()) {
      try {
        return CobaltProcessStateSummaryJni.get().computePersistentHash(data, length);
      } catch (UnsatisfiedLinkError e) {
        Log.w(TAG, "Native library initialized but JNI method missing", e);
      }
    }
    return computePersistentHashJava(data, length);
  }

  @CalledByNative
  public static void setProcessStateSummary(byte[] summaryBytes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;
    if (summaryBytes != null && summaryBytes.length > MAX_SUMMARY_BYTES) {
      Log.w(TAG, "Process state summary exceeds 128 bytes limit.");
      return;
    }
    sLastSummaryBytes = summaryBytes != null ? summaryBytes.clone() : null;
    try {
      ActivityManager am = getActivityManager();
      if (am != null) ApiHelperForR.setProcessStateSummary(am, summaryBytes);
    } catch (Exception e) {
      Log.e(TAG, "Failed to setProcessStateSummary: ", e);
    }
  }

  /** Sets kFlagStartupGuardTriggeredKill in the process state summary before watchdog kill. */
  public static void setStartupGuardTriggeredKill() {
    setStartupGuardTriggeredKill(0L, 0);
  }

  /** Sets kFlagStartupGuardTriggeredKill with milestone data before watchdog kill. */
  public static void setStartupGuardTriggeredKill(long startupStatus, int highestMilestone) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;
    try {
      ActivityManager am = getActivityManager();
      if (am == null) return;
      byte[] bytes = sLastSummaryBytes;
      if (bytes != null && bytes.length >= TOTAL_PAYLOAD_LENGTH) {
        bytes = bytes.clone();
        bytes[FLAGS_BYTE_INDEX] =
            (byte) (bytes[FLAGS_BYTE_INDEX] | FLAG_STARTUP_GUARD_TRIGGERED_KILL);
      } else {
        bytes = new byte[TOTAL_PAYLOAD_LENGTH];
        bytes[MAGIC_BYTE_INDEX] = MAGIC_BYTE;
        bytes[VERSION_BYTE_INDEX] = VERSION_BYTE;
        bytes[FLAGS_BYTE_INDEX] = FLAG_STARTUP_GUARD_TRIGGERED_KILL;
      }

      for (int i = 0; i < 8; ++i) {
        bytes[STARTUP_MILESTONES_BYTE_INDEX + i] = (byte) ((startupStatus >>> (i * 8)) & 0xFF);
      }
      bytes[HIGHEST_MILESTONE_BYTE_INDEX] = (byte) (highestMilestone & 0xFF);

      int checksum = computePersistentHash(bytes, CHECKSUM_DATA_LENGTH);
      bytes[12] = (byte) (checksum & 0xFF);
      bytes[13] = (byte) ((checksum >> 8) & 0xFF);
      bytes[14] = (byte) ((checksum >> 16) & 0xFF);
      bytes[15] = (byte) ((checksum >> 24) & 0xFF);

      sLastSummaryBytes = bytes;
      ApiHelperForR.setProcessStateSummary(am, bytes);
    } catch (Exception e) {
      Log.e(TAG, "Failed to setStartupGuardTriggeredKill: ", e);
    }
  }

  @CalledByNative
  public static @Nullable byte[] getPriorSessionProcessStateSummary() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return null;
    try {
      ActivityManager am = getActivityManager();
      if (am == null) return null;
      ApplicationExitInfo info = ApiHelperForR.getLatestApplicationExitInfo(am);
      return info != null ? ApiHelperForR.getProcessStateSummary(info) : null;
    } catch (Exception e) {
      Log.e(TAG, "Failed to getHistoricalProcessExitReasons summary: ", e);
      return null;
    }
  }

  /** Consolidates querying the latest exit reason and summary in a single Binder call. */
  @CalledByNative
  public static @Nullable byte[] recordLatestExitReasonAndGetSummary(
      String umaName, @Nullable int[] outExitReason) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return null;
    try {
      ActivityManager am = getActivityManager();
      if (am == null) return null;
      ApplicationExitInfo info = ApiHelperForR.getLatestApplicationExitInfo(am);
      if (info == null) return null;
      int systemReason = ApiHelperForR.getReason(info);
      Integer exitReason = ApiHelperForR.convertToExitReason(systemReason);
      if (exitReason != null && outExitReason != null && outExitReason.length > 0) {
        outExitReason[0] = exitReason;
      }
      return ApiHelperForR.getProcessStateSummary(info);
    } catch (Exception e) {
      Log.e(TAG, "Failed to recordLatestExitReasonAndGetSummary: ", e);
      return null;
    }
  }

  @NativeMethods
  public interface Natives {
    int computePersistentHash(byte[] data, int length);
  }
}
