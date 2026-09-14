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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import java.util.Collections;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link CobaltProcessStateSummary}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 30)
public class CobaltProcessStateSummaryTest {
  private ActivityManager mMockActivityManager;

  @Before
  public void setUp() {
    mMockActivityManager = mock(ActivityManager.class);
    CobaltProcessStateSummary.setActivityManagerForTesting(mMockActivityManager);
  }

  @After
  public void tearDown() {
    CobaltProcessStateSummary.setActivityManagerForTesting(null);
  }

  @Test
  public void testSetProcessStateSummary_validPayload() {
    byte[] payload = new byte[20];
    payload[0] = (byte) 0xCB;
    payload[1] = 1;

    CobaltProcessStateSummary.setProcessStateSummary(payload);
    verify(mMockActivityManager).setProcessStateSummary(payload);
  }

  @Test
  public void testSetProcessStateSummary_exceedsMaxBytes_rejected() {
    byte[] oversized = new byte[129];
    CobaltProcessStateSummary.setProcessStateSummary(oversized);
    verify(mMockActivityManager, never()).setProcessStateSummary(oversized);
  }

  @Test
  public void testSetProcessStateSummary_nullPayload_clearsSummary() {
    CobaltProcessStateSummary.setProcessStateSummary(null);
    verify(mMockActivityManager).setProcessStateSummary(null);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_queriesWithPidZero() {
    byte[] expectedSummary = new byte[] {(byte) 0xCB, 1, 0, 0};
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getPid()).thenReturn(1234);
    when(mockExitInfo.getProcessStateSummary()).thenReturn(expectedSummary);
    when(mockExitInfo.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);

    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    byte[] summary = CobaltProcessStateSummary.getPriorSessionProcessStateSummary();
    assertThat(summary).isEqualTo(expectedSummary);

    int[] outExitReason = new int[1];
    CobaltProcessStateSummary.recordLatestExitReasonAndGetSummary("TestUma", outExitReason);
    int exitReason = outExitReason[0];
    assertThat(exitReason).isAtLeast(0);

    int recordedReason =
        CobaltProcessStateSummary.recordLatestExitReasonToUma(
            "Cobalt.Stability.Android.SystemExitReason");
    assertThat(recordedReason).isAtLeast(0);

    byte[] summaryFromConsolidated =
        CobaltProcessStateSummary.recordLatestExitReasonAndGetSummary(
            "Cobalt.Stability.Android.SystemExitReason");
    assertThat(summaryFromConsolidated).isEqualTo(expectedSummary);
    assertThat(CobaltProcessStateSummary.getWasLowMemoryKilled()).isTrue();
  }

  @Test
  public void testGetWasLowMemoryKilled_otherReason_returnsFalse() {
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getReason()).thenReturn(ApplicationExitInfo.REASON_CRASH);

    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    assertThat(CobaltProcessStateSummary.getWasLowMemoryKilled()).isFalse();
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_emptyList_returnsNull() {
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.emptyList());

    assertThat(CobaltProcessStateSummary.getPriorSessionProcessStateSummary()).isNull();
    assertThat(CobaltProcessStateSummary.getWasLowMemoryKilled()).isFalse();
    int[] outExitReason = new int[1];
    outExitReason[0] = -1;
    CobaltProcessStateSummary.recordLatestExitReasonAndGetSummary("TestUma", outExitReason);
    assertThat(outExitReason[0]).isEqualTo(-1);
    assertThat(
            CobaltProcessStateSummary.recordLatestExitReasonToUma(
                "Cobalt.Stability.Android.SystemExitReason"))
        .isEqualTo(-1);
    assertThat(
            CobaltProcessStateSummary.recordLatestExitReasonAndGetSummary(
                "Cobalt.Stability.Android.SystemExitReason"))
        .isNull();
  }

  @Test
  public void testConvertToExitReason_delegatesToProcessExitReasonFromSystem() {
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_LOW_MEMORY))
        .isEqualTo(ProcessExitReasonFromSystem.ExitReason.REASON_LOW_MEMORY);
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_ANR))
        .isEqualTo(ProcessExitReasonFromSystem.ExitReason.REASON_ANR);
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_CRASH))
        .isEqualTo(ProcessExitReasonFromSystem.ExitReason.REASON_CRASH);
    assertThat(CobaltProcessStateSummary.convertToExitReason(-1))
        .isEqualTo(ProcessExitReasonFromSystem.ExitReason.REASON_API_FAILED);
  }
}
