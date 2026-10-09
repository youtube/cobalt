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
import dev.cobalt.util.ProcessExitReasonHelper;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for {@link CobaltProcessStateSummary}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 30)
public class CobaltProcessStateSummaryTest {

  private ActivityManager mMockActivityManager;

  @Before
  public void setUp() {
    CobaltProcessStateSummary.resetForTesting();
    mMockActivityManager = mock(ActivityManager.class);
    ProcessExitReasonHelper.setActivityManagerForTesting(mMockActivityManager);
  }

  @After
  public void tearDown() {
    CobaltProcessStateSummary.resetForTesting();
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
    byte[] testBytes = "hello world 1234567890".getBytes(StandardCharsets.UTF_8);
    assertThat(CobaltProcessStateSummary.computePersistentHashJava(testBytes, testBytes.length))
        .isEqualTo(0x75ed7b59);
  }

  @Test
  public void testSetProcessStateSummary_validPayload() {
    byte[] payload = new byte[16];
    payload[0] = CobaltProcessStateSummary.MAGIC_BYTE;
    payload[1] = CobaltProcessStateSummary.VERSION_BYTE;
    payload[2] = CobaltProcessStateSummary.FLAG_STARTUP_GUARD_ARMED;

    CobaltProcessStateSummary.setProcessStateSummary(payload);
    verify(mMockActivityManager).setProcessStateSummary(payload);
  }

  @Test
  public void testSetProcessStateSummary_ignoresOversizedPayload() {
    byte[] oversized = new byte[129];
    CobaltProcessStateSummary.setProcessStateSummary(oversized);
    verify(mMockActivityManager, never()).setProcessStateSummary(oversized);
  }

  @Test
  public void testSetStartupGuardTriggeredKill_stamps16ByteSnapshot() {
    long milestoneMask = (1L << 1) | (1L << 5) | (1L << 37);
    int highestMilestone = 37;

    CobaltProcessStateSummary.setStartupGuardTriggeredKill(milestoneMask, highestMilestone);

    byte[] expected = new byte[16];
    expected[0] = CobaltProcessStateSummary.MAGIC_BYTE;
    expected[1] = CobaltProcessStateSummary.VERSION_BYTE;
    expected[2] = CobaltProcessStateSummary.FLAG_STARTUP_GUARD_TRIGGERED_KILL;
    for (int i = 0; i < 8; ++i) {
      expected[3 + i] = (byte) ((milestoneMask >>> (i * 8)) & 0xFF);
    }
    expected[11] = 37;
    int checksum = CobaltProcessStateSummary.computePersistentHashJava(expected, 12);
    expected[12] = (byte) (checksum & 0xFF);
    expected[13] = (byte) ((checksum >> 8) & 0xFF);
    expected[14] = (byte) ((checksum >> 16) & 0xFF);
    expected[15] = (byte) ((checksum >> 24) & 0xFF);

    verify(mMockActivityManager).setProcessStateSummary(expected);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_retrievesSummary() {
    byte[] expectedSummary = new byte[] {CobaltProcessStateSummary.MAGIC_BYTE, 0x01, 0x02, 0x03};
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getProcessStateSummary()).thenReturn(expectedSummary);
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    byte[] retrieved = CobaltProcessStateSummary.getPriorSessionProcessStateSummary();
    assertThat(retrieved).isEqualTo(expectedSummary);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_returnsForegroundExitReason() {
    byte[] expectedSummary = new byte[] {CobaltProcessStateSummary.MAGIC_BYTE, 0x01, 0x02, 0x03};
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getProcessStateSummary()).thenReturn(expectedSummary);
    when(mockExitInfo.getImportance())
        .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND);
    when(mockExitInfo.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    int[] outReason = new int[1];
    byte[] retrieved = CobaltProcessStateSummary.getPriorSessionProcessStateSummary(outReason);
    assertThat(retrieved).isEqualTo(expectedSummary);
    assertThat(outReason[0]).isEqualTo(ProcessExitReasonHelper.ExitReason.REASON_LOW_MEMORY);
  }

  @Test
  public void testGetPriorSessionProcessStateSummary_ignoresBackgroundExitReason() {
    byte[] expectedSummary = new byte[] {CobaltProcessStateSummary.MAGIC_BYTE, 0x01, 0x02, 0x03};
    ApplicationExitInfo mockExitInfo = mock(ApplicationExitInfo.class);
    when(mockExitInfo.getProcessStateSummary()).thenReturn(expectedSummary);
    when(mockExitInfo.getImportance())
        .thenReturn(ActivityManager.RunningAppProcessInfo.IMPORTANCE_BACKGROUND);
    when(mockExitInfo.getReason()).thenReturn(ApplicationExitInfo.REASON_LOW_MEMORY);
    when(mMockActivityManager.getHistoricalProcessExitReasons(null, 0, 1))
        .thenReturn(Collections.singletonList(mockExitInfo));

    int[] outReason = new int[1];
    byte[] retrieved = CobaltProcessStateSummary.getPriorSessionProcessStateSummary(outReason);
    assertThat(retrieved).isEqualTo(expectedSummary);
    assertThat(outReason[0]).isEqualTo(-1);
  }
}
