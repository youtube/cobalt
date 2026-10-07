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

package dev.cobalt.util;

import static dev.cobalt.util.Log.TAG;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Queries Android {@link ActivityManager} for the historical process exit reason of the previous
 * Cobalt process session and records stability UMA histograms.
 *
 * <p>Lifetime/Ownership: Non-instantiable static utility class. Caches the queried {@link
 * ApplicationExitInfo} for the lifetime of the process to avoid redundant Binder IPC calls.
 *
 * <p>Threading model: Thread-safe. Static methods synchronize access to the cached exit info via an
 * internal lock and may be called from any thread.
 */
@JNINamespace("cobalt")
public final class ProcessExitReasonHelper {
  public static final String HISTOGRAM_SYSTEM_EXIT_REASON =
      "Cobalt.Stability.Android.SystemExitReason";
  public static final String HISTOGRAM_SYSTEM_EXIT_REASON_FOREGROUND =
      "Cobalt.Stability.Android.SystemExitReason.Foreground";

  private static ActivityManager sActivityManagerForTesting;

  private ProcessExitReasonHelper() {}

  // These values are persisted to logs (AndroidProcessExitReason in enums.xml).
  // Entries should not be renumbered and numeric values should never be reused.
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

  public static @Nullable Integer convertToExitReason(int systemReason) {
    switch (systemReason) {
      case -1:
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
        return null;
    }
  }

  /** Returns whether the previous process session was killed due to low memory. */
  public static boolean getWasLowMemoryKilled(@Nullable Context context) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return false;
    }
    ActivityManager am = ApiHelperForR.getActivityManager(context);
    return am != null && ApiHelperForR.getWasLowMemoryKilled(am);
  }

  /** Returns the ActivityManager using the application context (or test override). */
  @RequiresApi(Build.VERSION_CODES.R)
  public static @Nullable ActivityManager getActivityManager() {
    Context context = null;
    try {
      context = ContextUtils.getApplicationContext();
    } catch (Throwable ignored) {
    }
    return ApiHelperForR.getActivityManager(context);
  }

  /** Returns the cached latest historical ApplicationExitInfo for the process. */
  @RequiresApi(Build.VERSION_CODES.R)
  public static @Nullable ApplicationExitInfo getLatestProcessExitInfo() {
    return ApiHelperForR.getLatestProcessExitInfo(getActivityManager());
  }

  /** Records historical process exit reasons into UMA histograms. */
  @CalledByNative
  public static void recordHistoricalProcessExitReason() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    Context context = null;
    try {
      context = ContextUtils.getApplicationContext();
    } catch (Throwable ignored) {
    }
    ApiHelperForR.recordHistoricalProcessExitReason(context);
  }

  @RequiresApi(Build.VERSION_CODES.R)
  static final class ApiHelperForR {
    private static final Object sExitInfoLock = new Object();

    @GuardedBy("sExitInfoLock")
    private static boolean sExitInfoQueried;

    @GuardedBy("sExitInfoLock")
    private static @Nullable ApplicationExitInfo sCachedExitInfo;

    private ApiHelperForR() {}

    static @Nullable ActivityManager getActivityManager(@Nullable Context context) {
      if (sActivityManagerForTesting != null) {
        return sActivityManagerForTesting;
      }
      return context != null
          ? (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE)
          : null;
    }

    static @Nullable ApplicationExitInfo getLatestProcessExitInfo(@Nullable ActivityManager am) {
      synchronized (sExitInfoLock) {
        if (sExitInfoQueried) {
          return sCachedExitInfo;
        }
        if (am == null) {
          return null;
        }
        try {
          List<ApplicationExitInfo> reasons =
              am.getHistoricalProcessExitReasons(
                  /* package_name= */ null, /* pid= */ 0, /* maxNum= */ 1);
          if (reasons != null && !reasons.isEmpty() && reasons.get(0) != null) {
            sCachedExitInfo = reasons.get(0);
          }
          sExitInfoQueried = true;
        } catch (RuntimeException e) {
          Log.w(TAG, "Failed to get historical process exit reasons", e);
          sExitInfoQueried = true;
        }
        return sCachedExitInfo;
      }
    }

    static boolean getWasLowMemoryKilled(ActivityManager am) {
      ApplicationExitInfo info = getLatestProcessExitInfo(am);
      return info != null && info.getReason() == ApplicationExitInfo.REASON_LOW_MEMORY;
    }

    static void recordHistoricalProcessExitReason(@Nullable Context context) {
      ActivityManager am = getActivityManager(context);
      if (am == null) {
        Log.w(TAG, "ActivityManager is null, cannot get process exit reasons.");
        return;
      }
      ApplicationExitInfo info = getLatestProcessExitInfo(am);
      if (info == null) {
        return;
      }
      Integer exitReason = convertToExitReason(info.getReason());
      if (exitReason != null) {
        RecordHistogram.recordEnumeratedHistogram(
            HISTOGRAM_SYSTEM_EXIT_REASON, exitReason, ExitReason.NUM_ENTRIES);
        if (info.getImportance() <= ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND) {
          RecordHistogram.recordEnumeratedHistogram(
              HISTOGRAM_SYSTEM_EXIT_REASON_FOREGROUND, exitReason, ExitReason.NUM_ENTRIES);
        }
      }
    }

    static void resetForTesting() {
      synchronized (sExitInfoLock) {
        sExitInfoQueried = false;
        sCachedExitInfo = null;
      }
    }
  }

  @VisibleForTesting
  public static void setActivityManagerForTesting(@Nullable ActivityManager am) {
    sActivityManagerForTesting = am;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
      ApiHelperForR.resetForTesting();
    }
  }
}
