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
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.util.List;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Manages attaching compact process state summaries to the Android OS via
 * ActivityManager.setProcessStateSummary() (API 30+) during session lifetime, and recovering them
 * on subsequent startup via getHistoricalProcessExitReasons().
 *
 * <p>This is a static utility class with no state, so it has an infinite lifetime and does not
 * require ownership. Its methods are thread-safe and can be called from any thread.
 */
@JNINamespace("cobalt")
public class CobaltProcessStateSummary {
  private static final String TAG = "CobaltProcessSummary";
  private static final int MAX_SUMMARY_BYTES = 128;

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

  private static @Nullable ApplicationExitInfo getLatestApplicationExitInfo() {
    ActivityManager am = getActivityManager();
    if (am == null) {
      return null;
    }
    List<ApplicationExitInfo> reasons =
        am.getHistoricalProcessExitReasons(/* packageName= */ null, /* pid= */ 0, /* maxNum= */ 1);
    if (reasons == null || reasons.isEmpty()) {
      return null;
    }
    return reasons.get(0);
  }

  /** Converts Android system ApplicationExitInfo REASON_* constants to standard exit reasons. */
  public static @Nullable Integer convertToExitReason(int systemReason) {
    Integer exitReason = ProcessExitReasonFromSystem.convertToExitReason(systemReason);
    return exitReason != null ? exitReason : ProcessExitReasonFromSystem.ExitReason.REASON_OTHER;
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
    try {
      ActivityManager am = getActivityManager();
      if (am != null) {
        am.setProcessStateSummary(summaryBytes);
      }
    } catch (Exception e) {
      Log.e(TAG, "Failed to setProcessStateSummary: ", e);
    }
  }

  @CalledByNative
  public static @Nullable byte[] getPriorSessionProcessStateSummary() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return null;
    }
    try {
      ApplicationExitInfo info = getLatestApplicationExitInfo();
      return info != null ? info.getProcessStateSummary() : null;
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
      ApplicationExitInfo info = getLatestApplicationExitInfo();
      if (info == null) {
        return -1;
      }
      int systemReason = info.getReason();
      Integer exitReason = convertToExitReason(systemReason);
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
      ApplicationExitInfo info = getLatestApplicationExitInfo();
      if (info == null) {
        return null;
      }
      int systemReason = info.getReason();
      Integer exitReason = convertToExitReason(systemReason);
      if (exitReason != null) {
        if (outExitReason != null && outExitReason.length > 0) {
          outExitReason[0] = exitReason;
        }
      }
      return info.getProcessStateSummary();
    } catch (Exception e) {
      Log.e(TAG, "Failed to recordLatestExitReasonAndGetSummary: ", e);
      return null;
    }
  }

  @CalledByNative
  public static boolean getWasLowMemoryKilled() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return false;
    }
    try {
      ApplicationExitInfo info = getLatestApplicationExitInfo();
      return info != null && info.getReason() == ApplicationExitInfo.REASON_LOW_MEMORY;
    } catch (Exception e) {
      Log.w(TAG, "Failed to get historical process exit reasons: ", e);
      return false;
    }
  }
}
