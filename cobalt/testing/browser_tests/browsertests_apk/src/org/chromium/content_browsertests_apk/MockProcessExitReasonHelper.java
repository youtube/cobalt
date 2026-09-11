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

package org.chromium.content_browsertests_apk;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.chromium.base.ContextUtils;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.mockito.ArgumentMatchers;
import org.mockito.Mockito;

/**
 * Helper to mock ActivityManager and simulate system ApplicationExitInfo exit reasons for Cobalt
 * browsertests.
 */
@JNINamespace("cobalt")
public class MockProcessExitReasonHelper {

  public static final int REASON_EXIT_SELF = ApplicationExitInfo.REASON_EXIT_SELF;
  public static final int REASON_LOW_MEMORY = ApplicationExitInfo.REASON_LOW_MEMORY;

  private static final Map<Integer, Integer> sPidToReason = new ConcurrentHashMap<>();
  private static ActivityManager sMockAm;

  private static ApplicationExitInfo createApplicationExitInfo(int pid, int reason) {
    try {
      ApplicationExitInfo info;
      try {
        Constructor<ApplicationExitInfo> ctor = ApplicationExitInfo.class.getDeclaredConstructor();
        ctor.setAccessible(true);
        info = ctor.newInstance();
      } catch (Throwable t) {
        Field unsafeField = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        unsafeField.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) unsafeField.get(null);
        info = (ApplicationExitInfo) unsafe.allocateInstance(ApplicationExitInfo.class);
      }

      boolean pidSet = false;
      try {
        Method setPid = ApplicationExitInfo.class.getMethod("setPid", int.class);
        setPid.setAccessible(true);
        setPid.invoke(info, pid);
        pidSet = true;
      } catch (Throwable ignored) {
      }
      if (!pidSet) {
        Field field = ApplicationExitInfo.class.getDeclaredField("mPid");
        field.setAccessible(true);
        field.setInt(info, pid);
      }

      boolean reasonSet = false;
      try {
        Method setReason = ApplicationExitInfo.class.getMethod("setReason", int.class);
        setReason.setAccessible(true);
        setReason.invoke(info, reason);
        reasonSet = true;
      } catch (Throwable ignored) {
      }
      if (!reasonSet) {
        Field field = ApplicationExitInfo.class.getDeclaredField("mReason");
        field.setAccessible(true);
        field.setInt(info, reason);
      }

      return info;
    } catch (Exception e) {
      throw new RuntimeException("Failed to create ApplicationExitInfo for testing", e);
    }
  }

  @CalledByNativeForTesting
  public static void setMockExitReasonForTesting(int pid, int reason) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    if (System.getProperty("org.mockito.android.target") == null) {
      Context context = ContextUtils.getApplicationContext();
      if (context != null && context.getCacheDir() != null) {
        System.setProperty("org.mockito.android.target", context.getCacheDir().getPath());
      }
    }
    sPidToReason.put(pid, reason);
    if (sMockAm == null) {
      sMockAm = Mockito.mock(ActivityManager.class);
      Mockito.doAnswer(
              invocation -> {
                int requestedPid = invocation.getArgument(1);
                Integer exitReason = sPidToReason.get(requestedPid);
                if (exitReason == null) {
                  return Collections.emptyList();
                }
                return Collections.singletonList(
                    createApplicationExitInfo(requestedPid, exitReason));
              })
          .when(sMockAm)
          .getHistoricalProcessExitReasons(
              ArgumentMatchers.nullable(String.class),
              ArgumentMatchers.anyInt(),
              ArgumentMatchers.anyInt());
      ProcessExitReasonFromSystem.setActivityManagerForTest(sMockAm);
    }
  }

  @CalledByNativeForTesting
  public static void resetForTesting() {
    sPidToReason.clear();
    sMockAm = null;
    ProcessExitReasonFromSystem.setActivityManagerForTest(null);
  }
}
