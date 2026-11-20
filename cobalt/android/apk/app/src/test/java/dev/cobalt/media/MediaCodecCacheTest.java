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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ComponentCallbacks2;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import dev.cobalt.media.MediaCodecBridge.MimeTypes;
import dev.cobalt.media.MediaCodecCache.CodecConfig;
import java.time.Duration;
import java.util.concurrent.TimeUnit;
import org.chromium.base.ContextUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

/** Tests for parking a video MediaCodec in MediaCodecCache and reusing it. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecCacheTest {
  private static final String DECODER_NAME = "c2.test.av1.decoder";
  private static final long FIRST_NATIVE_BRIDGE = 1000L;
  private static final long SECOND_NATIVE_BRIDGE = 2000L;

  private Surface mSurface;
  private MediaCodecBridge.Natives mNatives;

  /** Builds the parameters of a codec request, starting from a clear SDR 1080p AV1 request. */
  private class ConfigBuilder {
    String mMime = MimeTypes.VIDEO_AV1;
    String mDecoderName = DECODER_NAME;
    Surface mConfigSurface = mSurface;
    int mWidthHint = 1920;
    int mHeightHint = 1080;
    int mFps = 0;
    int mMaxWidth = 3840;
    int mMaxHeight = 2160;
    int mMaxVideoInputSize = 0;
    boolean mEnableFrameRendererListener = false;
    boolean mIsTunneling = false;
    boolean mHasCrypto = false;
    boolean mIsHdr = false;

    CodecConfig build() {
      return new CodecConfig(
          mMime,
          mDecoderName,
          mConfigSurface,
          mWidthHint,
          mHeightHint,
          mFps,
          mMaxWidth,
          mMaxHeight,
          mMaxVideoInputSize,
          mEnableFrameRendererListener,
          mIsTunneling,
          mHasCrypto,
          mIsHdr);
    }
  }

  /** A MediaCodecBridge on top of a mock MediaCodec. */
  private static class TestCodec {
    final MediaCodec mMediaCodec = mock(MediaCodec.class);
    final MediaCodecBridge mBridge;
    final MediaCodec.Callback mCallback;

    TestCodec(CodecConfig reuseConfig) {
      mBridge =
          new MediaCodecBridge(
              FIRST_NATIVE_BRIDGE,
              mMediaCodec,
              DECODER_NAME,
              TunnelModeAudioSessionId.NONE,
              /* enableFrameRendererListener= */ false);
      mBridge.setReuseConfigForTesting(reuseConfig);
      ArgumentCaptor<MediaCodec.Callback> callback =
          ArgumentCaptor.forClass(MediaCodec.Callback.class);
      verify(mMediaCodec).setCallback(callback.capture(), any(Handler.class));
      mCallback = callback.getValue();
      // As done after configure() when the codec is created.
      assertTrue(mBridge.start());
    }

    void verifyReleased() {
      verify(mMediaCodec).release();
    }

    void verifyNotReleased() {
      verify(mMediaCodec, never()).stop();
      verify(mMediaCodec, never()).release();
    }
  }

  @Before
  public void setUp() {
    ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
    mSurface = mock(Surface.class);
    when(mSurface.isValid()).thenReturn(true);
    mNatives = mock(MediaCodecBridge.Natives.class);
    MediaCodecBridgeJni.setInstanceForTesting(mNatives);
    // Run the idle timeout on the main looper, which the tests control.
    MediaCodecCache.resetForTesting(new Handler(Looper.getMainLooper()));
  }

  @After
  public void tearDown() {
    MediaCodecBridgeJni.setInstanceForTesting(null);
    MediaCodecCache.resetForTesting(null);
  }

  private static MediaCodec.CodecException newCodecException() {
    return ReflectionHelpers.callConstructor(
        MediaCodec.CodecException.class,
        ClassParameter.from(int.class, 0),
        ClassParameter.from(int.class, 0),
        ClassParameter.from(String.class, "test"));
  }

  private static MediaFormat videoFormat(int width, int height) {
    return MediaFormat.createVideoFormat(MimeTypes.VIDEO_AV1, width, height);
  }

  private TestCodec createAndPark() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    codec.mBridge.release();
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    return codec;
  }

  private MediaCodecBridge acquire(CodecConfig request) {
    return MediaCodecCache.acquire(
        SECOND_NATIVE_BRIDGE, request, /* skipVideoFramesOver60Fps= */ false);
  }

  @Test
  public void withoutReuseConfig_releaseStopsAndReleasesTheCodecOnly() {
    TestCodec codec = new TestCodec(/* reuseConfig= */ null);

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    InOrder inOrder = inOrder(codec.mMediaCodec);
    inOrder.verify(codec.mMediaCodec).stop();
    inOrder.verify(codec.mMediaCodec).release();
    // Releasing must not add a flush to what the native decoder does.
    verify(codec.mMediaCodec, never()).flush();
  }

  @Test
  public void eligibleCodec_isParkedFlushedAndNotReleased() {
    TestCodec codec = createAndPark();

    verify(codec.mMediaCodec).flush();
    // Only the start() of the creation: a parked codec stays flushed, so it delivers no callbacks.
    verify(codec.mMediaCodec, times(1)).start();
    codec.verifyNotReleased();
  }

  @Test
  public void codecFlushedByNativeDecoder_isNotFlushedAgainWhenParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    // The native decoder flushes the codec right before releasing it.
    assertEquals(MediaCodecStatus.OK, codec.mBridge.flush());

    codec.mBridge.release();

    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    verify(codec.mMediaCodec, times(1)).flush();
  }

  @Test
  public void codecRestartedAfterFlush_isFlushedWhenParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    // A seek flushes and restarts the codec.
    assertEquals(MediaCodecStatus.OK, codec.mBridge.flush());
    assertTrue(codec.mBridge.start());

    codec.mBridge.release();

    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    verify(codec.mMediaCodec, times(2)).flush();
  }

  @Test
  public void identicalRequest_reusesTheParkedCodec() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1920, 1080));
    codec.mBridge.release();

    MediaCodecBridge reused = acquire(new ConfigBuilder().build());

    assertSame(codec.mBridge, reused);
    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    // Restarted, and the new owner is told the output format, as it would be by a new codec.
    verify(codec.mMediaCodec, times(2)).start();
    verify(mNatives).onMediaCodecOutputFormatChanged(SECOND_NATIVE_BRIDGE);
    codec.verifyNotReleased();
  }

  @Test
  public void reusedCodecThatNeverReportedItsFormat_reportsItItself() {
    // The first playback ended before the codec reported its output format.
    TestCodec codec = createAndPark();

    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));

    // The codec still reports its first format, so none is made up for the new owner.
    verify(mNatives, never()).onMediaCodecOutputFormatChanged(SECOND_NATIVE_BRIDGE);
    ShadowLooper.idleMainLooper();
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1920, 1080));
    verify(mNatives).onMediaCodecOutputFormatChanged(SECOND_NATIVE_BRIDGE);
  }

  @Test
  public void reusedCodecWhoseOutputSizeChanges_isNotParkedAgain() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1920, 1080));
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 0, info);
    codec.mBridge.release();
    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));
    ShadowLooper.idleMainLooper();

    // The second playback switches to another resolution before its first output.
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1280, 720));
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 0, info);
    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void reusedCodec_canBeParkedAndReusedAgain() {
    TestCodec codec = createAndPark();
    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));

    codec.mBridge.release();
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));

    verify(codec.mMediaCodec, times(2)).flush();
    verify(codec.mMediaCodec, times(3)).start();
    codec.verifyNotReleased();
  }

  @Test
  public void requestWithNothingParked_returnsNull() {
    assertNull(acquire(new ConfigBuilder().build()));
  }

  private void verifyMismatchEvicts(ConfigBuilder request) {
    TestCodec codec = createAndPark();

    assertNull(acquire(request.build()));

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
    verify(codec.mMediaCodec, times(1)).start();
  }

  @Test
  public void requestDifferingInAnyParameter_releasesTheParkedCodec() {
    ConfigBuilder request = new ConfigBuilder();
    request.mMime = MimeTypes.VIDEO_VP9;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mDecoderName = "c2.test.av1.decoder.secure";
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mConfigSurface = mock(Surface.class);
    when(request.mConfigSurface.isValid()).thenReturn(true);
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mWidthHint = 1280;
    request.mHeightHint = 720;
    verifyMismatchEvicts(request);

    // A smaller maximum size is a different configuration as well.
    request = new ConfigBuilder();
    request.mMaxWidth = 1920;
    request.mMaxHeight = 1080;
    verifyMismatchEvicts(request);

    // No maximum size means the largest size the decoder supports.
    request = new ConfigBuilder();
    request.mMaxWidth = -1;
    request.mMaxHeight = -1;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mFps = 60;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mMaxVideoInputSize = 4 * 1024 * 1024;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mEnableFrameRendererListener = true;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mIsTunneling = true;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mHasCrypto = true;
    verifyMismatchEvicts(request);

    request = new ConfigBuilder();
    request.mIsHdr = true;
    verifyMismatchEvicts(request);
  }

  @Test
  public void parkedCodecWhoseSurfaceBecameInvalid_isReleasedOnRequest() {
    TestCodec codec = createAndPark();
    when(mSurface.isValid()).thenReturn(false);

    assertNull(acquire(new ConfigBuilder().build()));

    codec.verifyReleased();
  }

  private void verifyNotParked(ConfigBuilder config) {
    TestCodec codec = new TestCodec(config.build());

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void tunnelSecureAndHdrCodecs_areNotParked() {
    ConfigBuilder config = new ConfigBuilder();
    config.mIsTunneling = true;
    verifyNotParked(config);

    config = new ConfigBuilder();
    config.mHasCrypto = true;
    verifyNotParked(config);

    config = new ConfigBuilder();
    config.mIsHdr = true;
    verifyNotParked(config);
  }

  @Test
  public void codecWithInvalidSurface_isNotParked() {
    when(mSurface.isValid()).thenReturn(false);
    verifyNotParked(new ConfigBuilder());
  }

  @Test
  public void codecThatReportedAnError_isNotParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    codec.mCallback.onError(codec.mMediaCodec, newCodecException());

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void codecWhoseFlushFails_isNotParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    doThrow(new IllegalStateException()).when(codec.mMediaCodec).flush();

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  /** Makes the codec's flush() take {@code durationMs}. */
  private static void flushTaking(TestCodec codec, long durationMs) {
    doAnswer(
            invocation -> {
              ShadowSystemClock.advanceBy(Duration.ofMillis(durationMs));
              return null;
            })
        .when(codec.mMediaCodec)
        .flush();
  }

  @Test
  public void codecWhoseFlushIsSlow_isNotParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    flushTaking(codec, MediaCodecCache.MAX_PARK_FLUSH_MS + 1);

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void codecWhoseFlushIsWithinTheLimit_isParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    flushTaking(codec, MediaCodecCache.MAX_PARK_FLUSH_MS);

    codec.mBridge.release();

    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyNotReleased();
  }

  @Test
  public void slowFlushByNativeDecoder_preventsParking() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    flushTaking(codec, MediaCodecCache.MAX_PARK_FLUSH_MS + 1);
    assertEquals(MediaCodecStatus.OK, codec.mBridge.flush());

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void errorOfParkedCodec_releasesIt() {
    TestCodec codec = createAndPark();

    codec.mCallback.onError(codec.mMediaCodec, newCodecException());

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
    // The previous owner is gone, so the error is not forwarded to anyone.
    verify(mNatives, never()).onMediaCodecError(anyLong(), anyBoolean(), anyBoolean(), any());
  }

  @Test
  public void parkedCodecThatFailsToRestart_isReleased() {
    TestCodec codec = createAndPark();
    doThrow(new IllegalStateException()).when(codec.mMediaCodec).start();

    assertNull(acquire(new ConfigBuilder().build()));

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
    // The new owner, which gets a new codec next, has not heard of the parked one.
    verify(mNatives, never()).onMediaCodecOutputFormatChanged(SECOND_NATIVE_BRIDGE);
  }

  @Test
  public void codecWhoseOutputSizeChanged_isNotParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1920, 1080));
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 0, info);
    // The stream switches to another resolution.
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1280, 720));

    codec.mBridge.release();

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void codecWhoseOutputSizeIsSetOnce_isParked() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    // Some codecs report a default format before the format of the stream.
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(320, 240));
    codec.mCallback.onOutputFormatChanged(codec.mMediaCodec, videoFormat(1920, 1080));
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 0, info);

    codec.mBridge.release();

    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
  }

  @Test
  public void surfaceNeedsClear_onceAfterCodecThatRenderedIsReleased() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    codec.mBridge.releaseOutputBuffer(0, /* render= */ true);
    codec.mBridge.release();
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    // The parked codec is still connected to the surface.
    assertFalse(MediaCodecCache.takeVideoSurfaceNeedsClear());

    ConfigBuilder request = new ConfigBuilder();
    request.mMime = MimeTypes.VIDEO_VP9;
    assertNull(acquire(request.build()));

    assertTrue(MediaCodecCache.takeVideoSurfaceNeedsClear());
    assertFalse(MediaCodecCache.takeVideoSurfaceNeedsClear());
  }

  @Test
  public void surfaceNeedsClear_afterCodecThatRenderedIsNotParked() {
    ConfigBuilder config = new ConfigBuilder();
    config.mIsHdr = true;
    TestCodec codec = new TestCodec(config.build());
    codec.mBridge.releaseOutputBuffer(0, /* render= */ true);

    codec.mBridge.release();

    codec.verifyReleased();
    assertTrue(MediaCodecCache.takeVideoSurfaceNeedsClear());
  }

  @Test
  public void surfaceDoesNotNeedClear_afterCodecThatRenderedNothing() {
    createAndPark();
    MediaCodecCache.discard("test");

    assertFalse(MediaCodecCache.takeVideoSurfaceNeedsClear());
  }

  @Test
  public void surfaceDoesNotNeedClear_afterSurfaceDestroyed() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    codec.mBridge.releaseOutputBuffer(0, /* render= */ true);
    codec.mBridge.release();

    MediaCodecCache.stopCaching("surface_destroyed");
    MediaCodecCache.onVideoSurfaceDestroyed();

    assertFalse(MediaCodecCache.takeVideoSurfaceNeedsClear());
  }

  @Test
  public void parkingASecondCodec_releasesTheFirst() {
    TestCodec first = createAndPark();
    TestCodec second = createAndPark();

    first.verifyReleased();
    second.verifyNotReleased();
    assertSame(second.mBridge, acquire(new ConfigBuilder().build()));
  }

  @Test
  public void discard_releasesTheParkedCodec() {
    TestCodec codec = createAndPark();

    MediaCodecCache.discard("test");

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void stopCaching_releasesTheParkedCodecAndPreventsParking() {
    TestCodec parked = createAndPark();

    MediaCodecCache.stopCaching("test");

    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    parked.verifyReleased();

    // A codec that was still in use is released when its player lets go of it.
    verifyNotParked(new ConfigBuilder());

    MediaCodecCache.allowCaching();
    createAndPark();
  }

  @Test
  public void parkedCodec_isReleasedAfterIdleTimeout() {
    TestCodec codec = createAndPark();

    ShadowLooper.idleMainLooper(MediaCodecCache.IDLE_TIMEOUT_MS - 1, TimeUnit.MILLISECONDS);
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyNotReleased();

    ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }

  @Test
  public void idleTimeout_restartsWhenTheCodecIsParkedAgain() {
    TestCodec codec = createAndPark();
    ShadowLooper.idleMainLooper(MediaCodecCache.IDLE_TIMEOUT_MS - 1, TimeUnit.MILLISECONDS);
    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));

    // Reused: the timeout of the first parking must not release it.
    ShadowLooper.idleMainLooper(MediaCodecCache.IDLE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    codec.verifyNotReleased();

    codec.mBridge.release();
    ShadowLooper.idleMainLooper(MediaCodecCache.IDLE_TIMEOUT_MS - 1, TimeUnit.MILLISECONDS);
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());
    ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
    codec.verifyReleased();
  }

  @Test
  public void callbacksOfThePreviousPlayer_areNotDeliveredToTheNextOne() {
    TestCodec codec = new TestCodec(new ConfigBuilder().build());
    ShadowLooper.idleMainLooper();
    codec.mBridge.release();
    assertSame(codec.mBridge, acquire(new ConfigBuilder().build()));

    // Callbacks that were posted to the main looper before the codec was flushed run now, after
    // the codec has been handed to the next player.
    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 3);
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 4, info);
    verify(mNatives, never()).onMediaCodecInputBufferAvailable(anyLong(), anyInt());
    verify(mNatives, never())
        .onMediaCodecOutputBufferAvailable(
            anyLong(), anyInt(), anyInt(), anyInt(), anyLong(), anyInt());

    // Everything posted before the flush has run by now; callbacks of the restarted codec follow.
    ShadowLooper.idleMainLooper();
    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 5);
    codec.mCallback.onOutputBufferAvailable(codec.mMediaCodec, 6, info);
    verify(mNatives).onMediaCodecInputBufferAvailable(SECOND_NATIVE_BRIDGE, 5);
    verify(mNatives)
        .onMediaCodecOutputBufferAvailable(
            SECOND_NATIVE_BRIDGE, 6, info.flags, info.offset, info.presentationTimeUs, info.size);
  }

  @Test
  public void callbacksOfParkedCodec_areDropped() {
    TestCodec codec = createAndPark();
    ShadowLooper.idleMainLooper();

    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 3);

    verify(mNatives, never()).onMediaCodecInputBufferAvailable(anyLong(), anyInt());
  }

  @Test
  public void lowMemory_releasesTheParkedCodec() {
    TestCodec codec = createAndPark();

    RuntimeEnvironment.getApplication()
        .onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE);
    ShadowLooper.idleMainLooper();
    assertTrue(MediaCodecCache.hasParkedCodecForTesting());

    RuntimeEnvironment.getApplication().onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW);
    ShadowLooper.idleMainLooper();
    assertFalse(MediaCodecCache.hasParkedCodecForTesting());
    codec.verifyReleased();
  }
}
