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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import android.media.AudioFormat;
import android.media.AudioTrack;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

/** Unit tests for {@link AudioTrackBridge}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AudioTrackBridgeTest {
  // 2^32 (4,294,967,296): one full epoch of the unsigned 32-bit hardware frame counter.
  private static final long TWO_POW_32 = 1L << 32;

  private AudioTrack mMockAudioTrack;
  private AudioTrackBridge mBridge;

  private long mSimulatedFramePosition;
  private long mSimulatedNanoTime;
  private boolean mSimulateTimestampSuccess;

  @Before
  public void setUp() {
    mMockAudioTrack = mock(AudioTrack.class);

    mSimulatedFramePosition = 0;
    mSimulatedNanoTime = 1_000_000L;
    mSimulateTimestampSuccess = true;

    doAnswer(
            invocation -> {
              android.media.AudioTimestamp ts = invocation.getArgument(0);
              if (mSimulateTimestampSuccess) {
                ts.framePosition = mSimulatedFramePosition;
                ts.nanoTime = mSimulatedNanoTime;
                return true;
              }
              return false;
            })
        .when(mMockAudioTrack)
        .getTimestamp(any(android.media.AudioTimestamp.class));

    mBridge =
        new AudioTrackBridge(
            AudioFormat.ENCODING_PCM_16BIT,
            /* sampleRate= */ 48000,
            /* channelCount= */ 2,
            /* maxSamplesPerWrite= */ 1024,
            /* preferredBufferSizeInBytes= */ 4096,
            TunnelModeAudioSessionId.NONE,
            /* isWebAudio= */ false);

    // The constructor creates a Robolectric-shadowed AudioTrack. Release it and replace it with
    // the mock so that each test fully controls the values reported by getTimestamp().
    AudioTrack shadowedAudioTrack = ReflectionHelpers.getField(mBridge, "mAudioTrack");
    if (shadowedAudioTrack != null) {
      shadowedAudioTrack.release();
    }
    ReflectionHelpers.setField(mBridge, "mAudioTrack", mMockAudioTrack);
  }

  @Test
  public void testAudioTimestampNormalAdvancement() {
    mSimulatedFramePosition = 0;
    mSimulatedNanoTime = 100_000L;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0L, ts.getFramePosition());
    assertEquals(100_000L, ts.getNanoTime());

    mSimulatedFramePosition = 480;
    mSimulatedNanoTime = 200_000L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(480L, ts.getFramePosition());
    assertEquals(200_000L, ts.getNanoTime());

    mSimulatedFramePosition = 960;
    mSimulatedNanoTime = 300_000L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(960L, ts.getFramePosition());
    assertEquals(300_000L, ts.getNanoTime());
  }

  @Test
  public void testAudioTimestampRolloverAcrossSigned31BitBoundary() {
    // 0x7FFFFFFE is 2,147,483,646 (2^31 - 2).
    mSimulatedFramePosition = 0x7FFFFFFEL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0x7FFFFFFEL, ts.getFramePosition());

    // 0x7FFFFFFF is 2,147,483,647 (2^31 - 1, signed 32-bit Integer.MAX_VALUE).
    mSimulatedFramePosition = 0x7FFFFFFFL;
    ts = mBridge.getAudioTimestamp();
    assertEquals(0x7FFFFFFFL, ts.getFramePosition());

    // 0x80000000 is 2,147,483,648 (2^31).
    mSimulatedFramePosition = 0x80000000L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(2_147_483_648L, ts.getFramePosition());

    // 0x80000001 is 2,147,483,649 (2^31 + 1).
    mSimulatedFramePosition = 0x80000001L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(2_147_483_649L, ts.getFramePosition());
  }

  @Test
  public void testAudioTimestampRolloverAcrossUnsigned32BitBoundary() {
    // 0xFFFFFFFF is 4,294,967,295 (2^32 - 1, unsigned 32-bit MAX_VALUE).
    mSimulatedFramePosition = 0xFFFFFFFFL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0xFFFFFFFFL, ts.getFramePosition());

    // Hardware counter wraps to 0x0 (0).
    mSimulatedFramePosition = 0x0L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(TWO_POW_32, ts.getFramePosition());

    // Continuous frames continue to advance seamlessly (0x100 is 256 / 2^8 frames).
    mSimulatedFramePosition = 0x100L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(TWO_POW_32 + 0x100L, ts.getFramePosition());
  }

  @Test
  public void testAudioTimestampMultipleRollovers() {
    // Advance in quarter-epoch steps (0x40000000 is 1,073,741,824 / 2^30). Every step is within
    // the 2^31 threshold, so the only wrap count increments are the drops from 0xC0000000 to 0x0.
    long[] rawSequence = {
      0x40000000L, 0x80000000L, 0xC0000000L, 0x0L, // 1st rollover.
      0x40000000L, 0x80000000L, 0xC0000000L, 0x0L, // 2nd rollover.
      0x40000000L,
    };
    long expected = 0;
    for (long raw : rawSequence) {
      mSimulatedFramePosition = raw;
      expected += 0x40000000L;
      assertEquals(expected, mBridge.getAudioTimestamp().getFramePosition());
    }
    // Final position is 9,663,676,416 (2 * 2^32 + 2^30).
    assertEquals(2 * TWO_POW_32 + 0x40000000L, expected);
  }

  @Test
  public void testAudioTimestampUpperBitsOfFramePositionAreDiscarded() {
    // -1L is 0xFFFFFFFF_FFFFFFFF, a sign-extended 0xFFFFFFFF (2^32 - 1). Only the low-order
    // 32 bits of AudioTimestamp.framePosition are valid.
    mSimulatedFramePosition = -1L;
    assertEquals(0xFFFFFFFFL, mBridge.getAudioTimestamp().getFramePosition());

    // 0x1_00000005 is 4,294,967,301 (2^32 + 5). The low-order 32 bits are 5, which is a
    // rollover from 2^32 - 1.
    mSimulatedFramePosition = 0x1_00000005L;
    assertEquals(TWO_POW_32 + 5L, mBridge.getAudioTimestamp().getFramePosition());
  }

  @Test
  public void testAudioTimestampBackwardJitterHoldsHighWaterMark() {
    mSimulatedFramePosition = 1000L;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(1000L, ts.getFramePosition());

    // Driver returns slight backward jitter (e.g. 10 frames back).
    mSimulatedFramePosition = 990L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(1000L, ts.getFramePosition());

    // Driver catches up and advances forward.
    mSimulatedFramePosition = 1020L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(1020L, ts.getFramePosition());
  }

  @Test
  public void testAudioTimestampBackwardJitterAcrossRolloverBoundary() {
    // 0xFFFFFFFE is 4,294,967,294 (2^32 - 2).
    mSimulatedFramePosition = 0xFFFFFFFEL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0xFFFFFFFEL, ts.getFramePosition());

    // Rollover to 0x2 (2, which is 2 frames past 2^32 boundary).
    mSimulatedFramePosition = 0x2L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(TWO_POW_32 + 2L, ts.getFramePosition());

    // Jitter backward across the boundary to 0xFFFFFFFE (4,294,967,294 / 2^32 - 2).
    // Crossing back below the boundary decrements the wrap count, so the continuous position
    // returns to epoch 0 (2^32 - 2) and the high-water mark holds at 2^32 + 2.
    mSimulatedFramePosition = 0xFFFFFFFEL;
    ts = mBridge.getAudioTimestamp();
    assertEquals(TWO_POW_32 + 2L, ts.getFramePosition());

    // Jitter resolves and advances forward past boundary to 0x5 (5 frames past boundary).
    mSimulatedFramePosition = 0x5L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(TWO_POW_32 + 5L, ts.getFramePosition());
  }

  @Test
  public void testAudioTimestampLargeForwardJumpWithoutPriorRolloverKeepsWrapCountNonNegative() {
    mSimulatedFramePosition = 0x10L;
    mBridge.getAudioTimestamp();

    // 0xFFFFFFF0 is 4,294,967,280 (2^32 - 16). The jump exceeds 2^31, but with no prior
    // rollover the wrap count is already 0, so it stays at 0 and the position is used as is.
    mSimulatedFramePosition = 0xFFFFFFF0L;
    assertEquals(0xFFFFFFF0L, mBridge.getAudioTimestamp().getFramePosition());
  }

  @Test
  public void testAudioTimestampFlushResetsContinuousPosition() {
    mSimulatedFramePosition = 5000L;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(5000L, ts.getFramePosition());

    mBridge.flush();

    // After flush, hardware resets to 0.
    mSimulatedFramePosition = 0L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(0L, ts.getFramePosition());

    mSimulatedFramePosition = 100L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(100L, ts.getFramePosition());
  }

  @Test
  public void testAudioTimestampFlushResetsWrapCount() {
    // 0xFFFFFFF0 is 4,294,967,280 (2^32 - 16).
    mSimulatedFramePosition = 0xFFFFFFF0L;
    mBridge.getAudioTimestamp();

    // Rollover to 0x10 (16), incrementing the wrap count to 1.
    mSimulatedFramePosition = 0x10L;
    assertEquals(TWO_POW_32 + 0x10L, mBridge.getAudioTimestamp().getFramePosition());

    mBridge.flush();

    // The wrap count is reset, so the position restarts in epoch 0.
    mSimulatedFramePosition = 100L;
    assertEquals(100L, mBridge.getAudioTimestamp().getFramePosition());
  }

  @Test
  public void testAudioTimestampFlushResetsLastRawPosition() {
    // 0xFFFFFFF0 is 4,294,967,280 (2^32 - 16).
    mSimulatedFramePosition = 0xFFFFFFF0L;
    mBridge.getAudioTimestamp();

    mBridge.flush();

    // The last raw position is reset, so a hardware position of 0 after flush is not
    // interpreted as a rollover from 0xFFFFFFF0.
    mSimulatedFramePosition = 0L;
    assertEquals(0L, mBridge.getAudioTimestamp().getFramePosition());
  }

  @Test
  public void testAudioTimestampTransientFailureReturnsLastContinuousPosition() {
    mSimulatedFramePosition = 2000L;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(2000L, ts.getFramePosition());

    // AudioTrack.getTimestamp fails intermittently.
    mSimulateTimestampSuccess = false;
    ts = mBridge.getAudioTimestamp();
    assertEquals(2000L, ts.getFramePosition());

    // Timestamp succeeds again.
    mSimulateTimestampSuccess = true;
    mSimulatedFramePosition = 2500L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(2500L, ts.getFramePosition());
  }
}
