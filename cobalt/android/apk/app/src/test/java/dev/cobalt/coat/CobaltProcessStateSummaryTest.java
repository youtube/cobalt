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
    CobaltProcessStateSummary.resetForTesting();
    mMockActivityManager = mock(ActivityManager.class);
    CobaltProcessStateSummary.setActivityManagerForTesting(mMockActivityManager);
  }

  @After
  public void tearDown() {
    CobaltProcessStateSummary.resetForTesting();
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
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_emptyList_returnsNull() {
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.emptyList());

    assertThat(CobaltProcessStateSummary.getPriorSessionProcessStateSummary()).isNull();
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
  public void testConvertToExitReason_matchesStandardExitReasons() {
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_LOW_MEMORY))
        .isEqualTo(CobaltProcessStateSummary.ExitReason.REASON_LOW_MEMORY);
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_ANR))
        .isEqualTo(CobaltProcessStateSummary.ExitReason.REASON_ANR);
    assertThat(CobaltProcessStateSummary.convertToExitReason(ApplicationExitInfo.REASON_CRASH))
        .isEqualTo(CobaltProcessStateSummary.ExitReason.REASON_CRASH);
    assertThat(
            CobaltProcessStateSummary.convertToExitReason(
                CobaltProcessStateSummary.SYSTEM_REASON_API_FAILED))
        .isEqualTo(CobaltProcessStateSummary.ExitReason.REASON_API_FAILED);
  }

  @Test
  public void testSetProcessStateSummary_exact128Bytes_accepted() {
    byte[] maxPayload = new byte[128];
    maxPayload[0] = (byte) 0xCB;
    maxPayload[1] = 2;
    CobaltProcessStateSummary.setProcessStateSummary(maxPayload);
    verify(mMockActivityManager).setProcessStateSummary(maxPayload);
  }

  @Test
  public void testSetProcessStateSummary_activityManagerException_doesNotCrash() {
    byte[] payload = new byte[40];
    org.mockito.Mockito.doThrow(new RuntimeException("IPC failure"))
        .when(mMockActivityManager)
        .setProcessStateSummary(payload);
    // Should catch exception and not re-throw.
    CobaltProcessStateSummary.setProcessStateSummary(payload);
  }

  @Test
  public void testComputePersistentHash_emptyOrNullReturnsZero() {
    assertThat(CobaltProcessStateSummary.computePersistentHash(null, 0)).isEqualTo(0);
    assertThat(CobaltProcessStateSummary.computePersistentHash(new byte[0], 0)).isEqualTo(0);
  }

  @Test
  public void testComputePersistentHashJava_knownVectors() {
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(null, 0)).isEqualTo(0);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(new byte[0], 0)).isEqualTo(0);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(new byte[] {'a'}, 1))
        .isEqualTo(0x115ea782);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(new byte[] {'a', 'b'}, 2))
        .isEqualTo(0x516b8b44);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(new byte[] {'a', 'b', 'c'}, 3))
        .isEqualTo(0xd2be198a);
    assertThat(
            CobaltProcessStateSummary.computePersistentHashJava(new byte[] {'a', 'b', 'c', 'd'}, 4))
        .isEqualTo(0xdad8b8db);
    byte[] testBytes = "hello world 1234567890".getBytes(java.nio.charset.StandardCharsets.UTF_8);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(testBytes, testBytes.length))
        .isEqualTo(0x75ed7b59);
  }

  @Test
  public void testSetStartupGuardTriggeredKill_updatesExistingSummary() {
    byte[] initialSummary = new byte[40];
    initialSummary[0] = (byte) 0xCB;
    initialSummary[1] = 2;
    initialSummary[3] = 0x04; // e.g. kFlagStartupGuardArmed

    CobaltProcessStateSummary.setProcessStateSummary(initialSummary);
    verify(mMockActivityManager).setProcessStateSummary(initialSummary);

    long startupStatus = 1L << 5;
    int highestMilestone = 5;
    CobaltProcessStateSummary.setStartupGuardTriggeredKill(startupStatus, highestMilestone);
    byte[] expectedUpdated = initialSummary.clone();
    expectedUpdated[3] =
        (byte) (0x04 | 0x08); // kFlagStartupGuardArmed | kFlagStartupGuardTriggeredKill
    for (int i = 0; i < 8; ++i) {
      expectedUpdated[24 + i] = (byte) ((startupStatus >>> (i * 8)) & 0xFF);
    }
    expectedUpdated[32] = 5;

    int checksum = CobaltProcessStateSummary.computePersistentHash(expectedUpdated, 36);
    assertThat(checksum).isNotEqualTo(0);
    expectedUpdated[36] = (byte) (checksum & 0xFF);
    expectedUpdated[37] = (byte) ((checksum >> 8) & 0xFF);
    expectedUpdated[38] = (byte) ((checksum >> 16) & 0xFF);
    expectedUpdated[39] = (byte) ((checksum >> 24) & 0xFF);

    verify(mMockActivityManager).setProcessStateSummary(expectedUpdated);
  }

  @Test
  public void testSetStartupGuardTriggeredKill_createsFallbackSummaryIfNoneSet() {
    long startupStatus = 1L << 7;
    int highestMilestone = 7;
    CobaltProcessStateSummary.setStartupGuardTriggeredKill(startupStatus, highestMilestone);
    byte[] expectedFallback = new byte[40];
    expectedFallback[0] = (byte) 0xCB;
    expectedFallback[1] = 2;
    expectedFallback[3] = 0x08; // kFlagStartupGuardTriggeredKill
    int pid = android.os.Process.myPid();
    expectedFallback[4] = (byte) (pid & 0xFF);
    expectedFallback[5] = (byte) ((pid >> 8) & 0xFF);
    expectedFallback[6] = (byte) ((pid >> 16) & 0xFF);
    expectedFallback[7] = (byte) ((pid >> 24) & 0xFF);

    for (int i = 0; i < 8; ++i) {
      expectedFallback[24 + i] = (byte) ((startupStatus >>> (i * 8)) & 0xFF);
    }
    expectedFallback[32] = 7;

    int checksum = CobaltProcessStateSummary.computePersistentHash(expectedFallback, 36);
    assertThat(checksum).isNotEqualTo(0);
    expectedFallback[36] = (byte) (checksum & 0xFF);
    expectedFallback[37] = (byte) ((checksum >> 8) & 0xFF);
    expectedFallback[38] = (byte) ((checksum >> 16) & 0xFF);
    expectedFallback[39] = (byte) ((checksum >> 24) & 0xFF);

    verify(mMockActivityManager).setProcessStateSummary(expectedFallback);
  }
}
