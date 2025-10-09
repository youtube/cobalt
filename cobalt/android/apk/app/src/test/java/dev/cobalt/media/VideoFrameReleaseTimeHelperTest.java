/*
 * Copyright 2022 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package dev.cobalt.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Junit tests that test the functionality of the VideoFrameReleaseTimeHelper class. */
@RunWith(RobolectricTestRunner.class)
@Config(
    shadows = {},
    manifest = Config.NONE)
public class VideoFrameReleaseTimeHelperTest {
    private static final long VSYNC_INTERVAL = 100L;
    private static final long VSYNC_SAMPLE_TIME = 1000L;

    // add function to calculate expected answers based on the 3 values.
    // need to also stub mVsyncSampler, VSYNC_OFFSET_PERCENTAGE
    // MIN_FRAMES_FOR_ADJUSTMENT, NANOS_PER_SECOND

  private VideoFrameReleaseTimeHelper mFrameReleaseHelper;

  @Before
  public void setUp() {
    mFrameReleaseHelper = new VideoFrameReleaseTimeHelper();
    mFrameReleaseHelper.enable();
  }

  @Test
  public void testFrameTimeDriftIsTooLarge() {
    // Set the sync frame; isDriftTooLarge relies on a sync frame being set.
    long actualAdjustedTime = mFrameReleaseHelper.adjustReleaseTime(
                    0,
                    0,
                    1.0
            );
    boolean isDriftTooLarge = mFrameReleaseHelper.isDriftTooLarge(10_000_000, 50_000_000L, 1.0);
    assertTrue(isDriftTooLarge);
  }

  @Test
  public void testFrameTimeDriftIsNotTooLarge() {
    long actualAdjustedTime = mFrameReleaseHelper.adjustReleaseTime(
                    0,
                    0,
                    1.0
            );
    boolean isDriftTooLarge = mFrameReleaseHelper.isDriftTooLarge(10_000_000L, 15_000_000L, 1.0);
    assertFalse(isDriftTooLarge);
  }

  @Test
  public void testSnapsToPreviousVsyncWhenCloser() {
        long releaseTime = 1010L;
        long expectedSnapTime = 1000L;

        // ACT
        long actualSnapTime = VideoFrameReleaseTimeHelper.closestVsync(
                releaseTime,
                VSYNC_SAMPLE_TIME,
                VSYNC_INTERVAL
        );

        // ASSERT
        assertEquals(
                "Should snap backward to the previous VSync",
                expectedSnapTime,
                actualSnapTime
        );
    }

  @Test
    public void snapsToNextVsyncWhenCloser() {
        long releaseTime = 1090L;
        long expectedSnapTime = 1100L;

        // ACT
        long actualSnapTime = VideoFrameReleaseTimeHelper.closestVsync(
                releaseTime,
                VSYNC_SAMPLE_TIME,
                VSYNC_INTERVAL
        );

        // ASSERT
        assertEquals(
                "Should snap forward to the next VSync",
                expectedSnapTime,
                actualSnapTime
        );
    }


  @Test
  public void testSimulationWithVsyncDisabled() {
        // The input data is the same as our previous test.
        long[][] frameInputs = new long[][]{
                {0L, 0L},                      // Frame 1
                {30_000_000L, 25_000_000L},     // Frame 2
                {60_000_000L, 70_000_000L},     // Frame 3
                {90_000_000L, 90_000_000L},     // Frame 4
                {120_000_000L, 122_000_000L},    // Frame 5
                {150_000_000L, 145_000_000L},    // Frame 6
                {180_000_000L, 190_000_000L},    // Frame 7 (First Adjustment)
                {210_000_000L, 210_000_000L},    // Frame 8
                {240_000_000L, 235_000_000L},    // Frame 9
                {270_000_000L, 280_000_000L}     // Frame 10
        };

        // NEW Expected Results:
        // Frames 1-6: The original jittery time is returned.
        // Frames 7-10: The calculated SMOOTHED time is returned, with NO snapping.
        long[] expectedResults = new long[]{
                0L,                   // Frame 1: Returns original time
                25_000_000L,          // Frame 2: Returns original time
                70_000_000L,          // Frame 3: Returns original time
                90_000_000L,          // Frame 4: Returns original time
                122_000_000L,         // Frame 5: Returns original time
                145_000_000L,         // Frame 6: Returns original time
                180_000_000L,         // Frame 7: Returns SMOOTHED time
                210_000_000L,         // Frame 8: Returns SMOOTHED time
                240_000_000L,         // Frame 9: Returns SMOOTHED time
                270_000_000L          // Frame 10: Returns SMOOTHED time
        };

        for (int i = 0; i < frameInputs.length; i++) {
            long idealPtsNs = frameInputs[i][0];
            long jitteryRealTimeNs = frameInputs[i][1];

            long actualAdjustedTime = mFrameReleaseHelper.adjustReleaseTime(
                    idealPtsNs / 1000,
                    jitteryRealTimeNs,
                    1.0
            );

            long expectedAdjustedTime = expectedResults[i];

            System.out.printf(
                "Frame %2d: Input Real Time: %-12d Expected: %-12d Actual: %-12d%n",
                (i + 1), jitteryRealTimeNs, expectedAdjustedTime, actualAdjustedTime
            );

            assertEquals("Mismatch on frame " + (i + 1), expectedAdjustedTime, actualAdjustedTime);
        }
    }
}
