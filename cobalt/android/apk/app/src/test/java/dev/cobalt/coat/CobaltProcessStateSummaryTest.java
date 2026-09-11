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
  public void testSetProcessStateSummary_nullPayload_handledGracefully() {
    CobaltProcessStateSummary.setProcessStateSummary(null);
    verify(mMockActivityManager, never()).setProcessStateSummary(null);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_matchingPid() {
    int pid = 1234;
    byte[] expectedSummary = new byte[] {(byte) 0xCB, 1, 0, 0};
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getPid()).thenReturn(pid);
    when(mockExitInfo.getProcessStateSummary()).thenReturn(expectedSummary);
    when(mockExitInfo.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);

    when(mMockActivityManager.getHistoricalProcessExitReasons(null, pid, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    byte[] summary = CobaltProcessStateSummary.getPriorSessionProcessStateSummary(pid);
    assertThat(summary).isEqualTo(expectedSummary);

    int exitReason = CobaltProcessStateSummary.getPriorSessionExitReason(pid);
    assertThat(exitReason).isAtLeast(0);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_emptyList_returnsNull() {
    int pid = 5678;
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, pid, 1))
        .thenReturn(Collections.emptyList());

    assertThat(CobaltProcessStateSummary.getPriorSessionProcessStateSummary(pid)).isNull();
    assertThat(CobaltProcessStateSummary.getPriorSessionExitReason(pid)).isEqualTo(-1);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_pidMismatch_returnsNull() {
    int pid = 1234;
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getPid()).thenReturn(9999);
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, pid, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    assertThat(CobaltProcessStateSummary.getPriorSessionProcessStateSummary(pid)).isNull();
    assertThat(CobaltProcessStateSummary.getPriorSessionExitReason(pid)).isEqualTo(-1);
  }
}
