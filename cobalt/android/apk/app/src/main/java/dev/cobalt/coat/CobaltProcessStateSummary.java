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
import java.util.List;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Manages attaching compact process state summaries to the Android OS via
 * ActivityManager.setProcessStateSummary() (API 30+) during session lifetime, and recovering them
 * on subsequent startup via getHistoricalProcessExitReasons().
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
    Context context = ContextUtils.getApplicationContext();
    if (context == null) {
      return null;
    }
    return (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
  }

  @VisibleForTesting
  public static void setActivityManagerForTesting(ActivityManager am) {
    sActivityManagerForTesting = am;
  }

  @CalledByNative
  public static void setProcessStateSummary(byte[] summaryBytes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    if (summaryBytes == null || summaryBytes.length > MAX_SUMMARY_BYTES) {
      Log.w(TAG, "Process state summary exceeds 128 bytes limit or is null.");
      return;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am != null) {
        am.setProcessStateSummary(summaryBytes);
      }
    } catch (Throwable t) {
      Log.e(TAG, "Failed to setProcessStateSummary: ", t);
    }
  }

  @CalledByNative
  public static @Nullable byte[] getPriorSessionProcessStateSummary(int pid) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return null;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return null;
      }
      List<ApplicationExitInfo> reasons =
          am.getHistoricalProcessExitReasons(/* packageName= */ null, pid, /* maxNum= */ 1);
      if (reasons == null || reasons.isEmpty() || reasons.get(0) == null) {
        return null;
      }
      if (reasons.get(0).getPid() != pid) {
        return null;
      }
      return reasons.get(0).getProcessStateSummary();
    } catch (Throwable t) {
      Log.e(TAG, "Failed to getHistoricalProcessExitReasons summary: ", t);
      return null;
    }
  }

  @CalledByNative
  public static int getPriorSessionExitReason(int pid) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return -1;
    }
    try {
      ActivityManager am = getActivityManager();
      if (am == null) {
        return -1;
      }
      List<ApplicationExitInfo> reasons =
          am.getHistoricalProcessExitReasons(/* packageName= */ null, pid, /* maxNum= */ 1);
      if (reasons == null || reasons.isEmpty() || reasons.get(0) == null) {
        return -1;
      }
      if (reasons.get(0).getPid() != pid) {
        return -1;
      }
      Integer exitReason =
          ProcessExitReasonFromSystem.convertToExitReason(reasons.get(0).getReason());
      return exitReason != null ? exitReason : -1;
    } catch (Throwable t) {
      Log.e(TAG, "Failed to getHistoricalProcessExitReasons reason: ", t);
      return -1;
    }
  }
}
