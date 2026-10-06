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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import java.util.Collections;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Unit tests for {@link ProcessExitReasonHelper}. */
@RunWith(RobolectricTestRunner.class)
public class ProcessExitReasonHelperTest {
  private Context mContext;

  @Before
  public void setUp() {
    ProcessExitReasonHelper.setActivityManagerForTesting(null);
    UmaRecorderHolder.resetForTesting();
    mContext = RuntimeEnvironment.getApplication();
  }

  @After
  public void tearDown() {
    ProcessExitReasonHelper.setActivityManagerForTesting(null);
    UmaRecorderHolder.resetForTesting();
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void getWasLowMemoryKilled_lowMemory() {
    ActivityManager am = mock(ActivityManager.class);
    ApplicationExitInfo info = mock(ApplicationExitInfo.class);
    when(info.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1)))
        .thenReturn(Collections.singletonList(info));
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    assertTrue(ProcessExitReasonHelper.getWasLowMemoryKilled(mContext));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void getWasLowMemoryKilled_otherReason() {
    ActivityManager am = mock(ActivityManager.class);
    ApplicationExitInfo info = mock(ApplicationExitInfo.class);
    when(info.getReason()).thenReturn(ApplicationExitInfo.REASON_CRASH);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1)))
        .thenReturn(Collections.singletonList(info));
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    assertFalse(ProcessExitReasonHelper.getWasLowMemoryKilled(mContext));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void getWasLowMemoryKilled_nullReasons() {
    ActivityManager am = mock(ActivityManager.class);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1))).thenReturn(null);
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    assertFalse(ProcessExitReasonHelper.getWasLowMemoryKilled(mContext));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void getWasLowMemoryKilled_cachesResultAcrossCalls() {
    ActivityManager am = mock(ActivityManager.class);
    ApplicationExitInfo info = mock(ApplicationExitInfo.class);
    when(info.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1)))
        .thenReturn(Collections.singletonList(info));
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    assertTrue(ProcessExitReasonHelper.getWasLowMemoryKilled(mContext));
    verify(am, times(1)).getHistoricalProcessExitReasons(isNull(), eq(0), eq(1));

    assertTrue(ProcessExitReasonHelper.getWasLowMemoryKilled(mContext));
    ProcessExitReasonHelper.recordHistoricalProcessExitReason();
    verify(am, times(1)).getHistoricalProcessExitReasons(isNull(), eq(0), eq(1));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void recordHistoricalProcessExitReason_recordsUmaForeground() {
    ActivityManager am = mock(ActivityManager.class);
    ApplicationExitInfo info = mock(ApplicationExitInfo.class);
    when(info.getReason()).thenReturn(ApplicationExitInfo.REASON_ANR);
    when(info.getImportance())
        .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1)))
        .thenReturn(Collections.singletonList(info));
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    ProcessExitReasonHelper.recordHistoricalProcessExitReason();

    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            ProcessExitReasonHelper.HISTOGRAM_SYSTEM_EXIT_REASON,
            ProcessExitReasonHelper.ExitReason.REASON_ANR));
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            ProcessExitReasonHelper.HISTOGRAM_SYSTEM_EXIT_REASON_FOREGROUND,
            ProcessExitReasonHelper.ExitReason.REASON_ANR));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.R)
  public void recordHistoricalProcessExitReason_recordsUmaBackground() {
    ActivityManager am = mock(ActivityManager.class);
    ApplicationExitInfo info = mock(ApplicationExitInfo.class);
    when(info.getReason()).thenReturn(ApplicationExitInfo.REASON_CRASH);
    when(info.getImportance())
        .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_BACKGROUND);
    when(am.getHistoricalProcessExitReasons(isNull(), eq(0), eq(1)))
        .thenReturn(Collections.singletonList(info));
    ProcessExitReasonHelper.setActivityManagerForTesting(am);

    ProcessExitReasonHelper.recordHistoricalProcessExitReason();

    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            ProcessExitReasonHelper.HISTOGRAM_SYSTEM_EXIT_REASON,
            ProcessExitReasonHelper.ExitReason.REASON_CRASH));
    assertEquals(
        0,
        RecordHistogram.getHistogramTotalCountForTesting(
            ProcessExitReasonHelper.HISTOGRAM_SYSTEM_EXIT_REASON_FOREGROUND));
  }

  @Test
  public void testConvertToExitReason() {
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_API_FAILED),
        ProcessExitReasonHelper.convertToExitReason(-1));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_ANR),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_ANR));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_CRASH),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_CRASH));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_CRASH_NATIVE),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_CRASH_NATIVE));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_DEPENDENCY_DIED),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_DEPENDENCY_DIED));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_EXCESSIVE_RESOURCE_USAGE),
        ProcessExitReasonHelper.convertToExitReason(
            ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_EXIT_SELF),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_EXIT_SELF));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_INITIALIZATION_FAILURE),
        ProcessExitReasonHelper.convertToExitReason(
            ApplicationExitInfo.REASON_INITIALIZATION_FAILURE));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_LOW_MEMORY),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_LOW_MEMORY));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_OTHER),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_OTHER));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_PERMISSION_CHANGE),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_PERMISSION_CHANGE));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_SIGNALED),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_SIGNALED));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_UNKNOWN),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_UNKNOWN));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_USER_REQUESTED),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_USER_REQUESTED));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_USER_STOPPED),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_USER_STOPPED));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_FREEZER),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_FREEZER));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_PACKAGE_STATE_CHANGE),
        ProcessExitReasonHelper.convertToExitReason(
            ApplicationExitInfo.REASON_PACKAGE_STATE_CHANGE));
    assertEquals(
        Integer.valueOf(ProcessExitReasonHelper.ExitReason.REASON_PACKAGE_UPDATED),
        ProcessExitReasonHelper.convertToExitReason(ApplicationExitInfo.REASON_PACKAGE_UPDATED));
    assertNull(ProcessExitReasonHelper.convertToExitReason(99999));
  }
}
