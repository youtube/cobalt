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
import static org.mockito.Mockito.when;

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

  private AudioTrack mMockAudioTrack;
  private AudioTrackBridge mBridge;

  private long mSimulatedFramePosition;
  private long mSimulatedNanoTime;
  private boolean mSimulateTimestampSuccess;

  @Before
  public void setUp() {
    mMockAudioTrack = mock(AudioTrack.class);
    when(mMockAudioTrack.getState()).thenReturn(AudioTrack.STATE_INITIALIZED);

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
            /* tunnelModeAudioSessionId= */ -1,
            /* isWebAudio= */ false);

    ReflectionHelpers.setField(mBridge, "mAudioTrack", mMockAudioTrack);
  }

  @Test
  public void testNormalAdvancement() {
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
  public void testRolloverAcrossSigned31BitBoundary() {
    // 0x7FFFFFFE is 2,147,483,646 (2^31 - 2).
    mSimulatedFramePosition = 0x7FFFFFFEL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0x7FFFFFFEL, ts.getFramePosition());

    // 0x7FFFFFFF is 2,147,483,647 (2^31 - 1, signed 32-bit Integer.MAX_VALUE).
    // The previous implementation masked with 0x7FFFFFFF, stripping bit 31 and freezing here.
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
  public void testRolloverAcrossUnsigned32BitBoundary() {
    // 0xFFFFFFFF is 4,294,967,295 (2^32 - 1, unsigned 32-bit MAX_VALUE).
    mSimulatedFramePosition = 0xFFFFFFFFL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0xFFFFFFFFL, ts.getFramePosition());

    // Hardware counter wraps to 0x0 (0).
    mSimulatedFramePosition = 0x0L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(4_294_967_296L, ts.getFramePosition());

    // Continuous frames continue to advance seamlessly (0x100 is 256 / 2^8 frames).
    mSimulatedFramePosition = 0x100L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(4_294_967_296L + 0x100L, ts.getFramePosition());
  }

  @Test
  public void testBackwardJitterIsIgnored() {
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
  public void testBackwardJitterAcrossRolloverBoundary() {
    // 0xFFFFFFFE is 4,294,967,294 (2^32 - 2).
    mSimulatedFramePosition = 0xFFFFFFFEL;
    AudioTrackBridge.AudioTimestamp ts = mBridge.getAudioTimestamp();
    assertEquals(0xFFFFFFFEL, ts.getFramePosition());

    // Rollover to 0x2 (2, which is 2 frames past 2^32 boundary).
    mSimulatedFramePosition = 0x2L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(4_294_967_296L + 2L, ts.getFramePosition());

    // Jitter backward across the boundary to 0xFFFFFFFE (4,294,967,294 / 2^32 - 2).
    mSimulatedFramePosition = 0xFFFFFFFEL;
    ts = mBridge.getAudioTimestamp();
    // Must NOT jump forward by 4.29B frames; monotonic latch should hold at 4_294_967_298L.
    assertEquals(4_294_967_296L + 2L, ts.getFramePosition());

    // Jitter resolves and advances forward past boundary to 0x5 (5 frames past boundary).
    mSimulatedFramePosition = 0x5L;
    ts = mBridge.getAudioTimestamp();
    assertEquals(4_294_967_296L + 5L, ts.getFramePosition());
  }

  @Test
  public void testFlushResetsContinuousPosition() {
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
  public void testTransientFailureReturnsLastContinuousPosition() {
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
