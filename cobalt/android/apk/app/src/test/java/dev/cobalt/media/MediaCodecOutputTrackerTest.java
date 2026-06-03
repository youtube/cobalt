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

package dev.cobalt.media;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.chromium.base.metrics.RecordHistogram;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit Tests for MediaCodecOutputTracker class. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecOutputTrackerTest {

  private MediaCodecOutputTracker mTracker;

  @Before
  public void setUp() {
    MediaCodecOutputTracker.resetForTesting();
    mTracker = MediaCodecOutputTracker.get();
  }

  @After
  public void tearDown() {
    MediaCodecOutputTracker.resetForTesting();
  }

  @Test
  public void testMemoryCalculation_SingleBridge() {
    MediaCodecBridge mockBridge = mock(MediaCodecBridge.class);
    when(mockBridge.getCurrentMediaFormatDimension()).thenReturn(1920 * 1080);
    when(mockBridge.sizeOfActiveOutputBuffers()).thenReturn(10);

    // Total: 2073600 * 1.5 * 10 = 31104000 bytes
    // 31104000 / (1024 * 1024) = 29.66 MiB -> 29 MiB recorded
    mTracker.register(mockBridge);
    mTracker.forceReportForTesting();

    assertEquals(1, RecordHistogram.getHistogramValueCountForTesting("Memory.Media.EstimatedDecodedBuffer", 29));
  }

  @Test
  public void testMemoryCalculation_MultipleBridges() {
    MediaCodecBridge bridge1 = mock(MediaCodecBridge.class);
    when(bridge1.getCurrentMediaFormatDimension()).thenReturn(1280 * 720); // 921600
    when(bridge1.sizeOfActiveOutputBuffers()).thenReturn(5);
    // 921600 * 1.5 * 5 = 6912000 bytes (~6.59 MiB)

    MediaCodecBridge bridge2 = mock(MediaCodecBridge.class);
    when(bridge2.getCurrentMediaFormatDimension()).thenReturn(1920 * 1080); // 2073600
    when(bridge2.sizeOfActiveOutputBuffers()).thenReturn(8);
    // 2073600 * 1.5 * 8 = 24883200 bytes (~23.73 MiB)

    // Total: 6912000 + 24883200 = 31795200 bytes
    // 31795200 / (1024 * 1024) = 30.32 MiB -> 30 MiB recorded
    mTracker.register(bridge1);
    mTracker.register(bridge2);
    mTracker.forceReportForTesting();

    assertEquals(1, RecordHistogram.getHistogramValueCountForTesting("Memory.Media.EstimatedDecodedBuffer", 30));
  }

  @Test
  public void testUnregisterRemovesBridge() {
    MediaCodecBridge bridge1 = mock(MediaCodecBridge.class);
    when(bridge1.getCurrentMediaFormatDimension()).thenReturn(1000 * 1000);
    when(bridge1.sizeOfActiveOutputBuffers()).thenReturn(1);
    // 1000000 * 1.5 * 1 = 1500000 bytes (~1.43 MiB)

    MediaCodecBridge bridge2 = mock(MediaCodecBridge.class);
    when(bridge2.getCurrentMediaFormatDimension()).thenReturn(2000 * 2000);
    when(bridge2.sizeOfActiveOutputBuffers()).thenReturn(1);
    // 4000000 * 1.5 * 1 = 6000000 bytes (~5.72 MiB)

    mTracker.register(bridge1);
    mTracker.register(bridge2);
    mTracker.unregister(bridge1);
    mTracker.forceReportForTesting();

    // Only bridge2 should be counted: 5.72 MiB -> 5 MiB
    assertEquals(1, RecordHistogram.getHistogramValueCountForTesting("Memory.Media.EstimatedDecodedBuffer", 5));
  }
}
