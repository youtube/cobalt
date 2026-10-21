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
import android.os.Build;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import dev.cobalt.util.ProcessExitReasonHelper;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Java bridge for attaching and retrieving the 16-byte process state summary via Android R+ (API
 * 30+) ActivityManager APIs, reusing {@link ProcessExitReasonHelper} for exit info queries.
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

  private static volatile byte[] sLastSummaryBytes;

  private CobaltProcessStateSummary() {}

  @VisibleForTesting
  public static void resetForTesting() {
    sLastSummaryBytes = null;
    ProcessExitReasonHelper.setActivityManagerForTesting(null);
  }

  private static int get16bits(byte[] data, int offset) {
    return ((data[offset + 1] & 0xFF) << 8) | (data[offset] & 0xFF);
  }

  /** Pure Java implementation of SuperFastHash (base::PersistentHash). */
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

  @CalledByNative
  public static void setProcessStateSummary(byte[] summaryBytes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;
    if (summaryBytes != null && summaryBytes.length > MAX_SUMMARY_BYTES) {
      Log.w(TAG, "Process state summary exceeds 128 bytes limit.");
      return;
    }
    sLastSummaryBytes = summaryBytes != null ? summaryBytes.clone() : null;
    try {
      ActivityManager am = ProcessExitReasonHelper.getActivityManager();
      if (am != null) {
        am.setProcessStateSummary(summaryBytes);
      }
    } catch (RuntimeException e) {
      Log.e(TAG, "Failed to setProcessStateSummary: ", e);
    }
  }

  /**
   * Sets {@link #FLAG_STARTUP_GUARD_TRIGGERED_KILL} in the process state summary before {@link
   * dev.cobalt.shell.StartupGuard} triggers a forced crash on startup timeout.
   */
  public static void setStartupGuardTriggeredKill() {
    setStartupGuardTriggeredKill(0L, 0);
  }

  /**
   * Sets {@link #FLAG_STARTUP_GUARD_TRIGGERED_KILL} with milestone data before {@link
   * dev.cobalt.shell.StartupGuard} triggers a forced crash on startup timeout.
   */
  public static void setStartupGuardTriggeredKill(long startupStatus, int highestMilestone) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return;
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

    int checksum = computePersistentHashJava(bytes, CHECKSUM_DATA_LENGTH);
    bytes[12] = (byte) (checksum & 0xFF);
    bytes[13] = (byte) ((checksum >> 8) & 0xFF);
    bytes[14] = (byte) ((checksum >> 16) & 0xFF);
    bytes[15] = (byte) ((checksum >> 24) & 0xFF);

    setProcessStateSummary(bytes);
  }

  public static @Nullable byte[] getPriorSessionProcessStateSummary() {
    return getPriorSessionProcessStateSummary(null);
  }

  /**
   * Retrieves the prior-session process state summary and foreground exit reason using the cached
   * {@link ApplicationExitInfo} from {@link ProcessExitReasonHelper}.
   */
  @CalledByNative
  public static @Nullable byte[] getPriorSessionProcessStateSummary(@Nullable int[] outExitReason) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return null;
    try {
      ApplicationExitInfo info = ProcessExitReasonHelper.getLatestProcessExitInfo();
      if (info == null) return null;
      if (outExitReason != null && outExitReason.length > 0) {
        Integer exitReason =
            info.getImportance() <= ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
                ? ProcessExitReasonHelper.convertToExitReason(info.getReason())
                : null;
        outExitReason[0] = (exitReason != null) ? exitReason : -1;
      }
      return info.getProcessStateSummary();
    } catch (RuntimeException e) {
      Log.e(TAG, "Failed to getPriorSessionProcessStateSummary: ", e);
      return null;
    }
  }
}
