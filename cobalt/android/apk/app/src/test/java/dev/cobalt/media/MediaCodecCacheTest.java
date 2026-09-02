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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.media.MediaCodec;
import android.view.Surface;
import dev.cobalt.media.MediaCodecBridge.CreateMediaCodecBridgeResult;
import dev.cobalt.media.MediaCodecBridge.MimeTypes;
import dev.cobalt.media.MediaCodecCache.CachedCodec;
import dev.cobalt.media.MediaCodecCache.CodecConfig;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for MediaCodecCache codec reuse behavior across playbacks. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecCacheTest {

  private Surface mMockSurface;
  private MediaCodec mMockMediaCodec;
  private MediaCodecBridge.Natives mMockNatives;
  private TestableMediaCodecBridge mBridge;
  private FakeSurfaceVisibility mVisibility;

  /** Records the video surface visibility changes requested by MediaCodecCache. */
  private static class FakeSurfaceVisibility implements MediaCodecCache.SurfaceVisibility {
    final List<String> mCalls = new ArrayList<>();
    boolean mWithNextFrameSupported = true;

    @Override
    public void setVisible(boolean visible) {
      mCalls.add("visible=" + visible);
    }

    @Override
    public boolean setVisibleWithNextFrame(boolean visible) {
      mCalls.add("withNextFrame=" + visible);
      return mWithNextFrameSupported;
    }
  }

  private static class TestableMediaCodecBridge extends MediaCodecBridge {
    boolean doReleaseCalled = false;

    TestableMediaCodecBridge(MediaCodec mediaCodec, String codecName) {
      this(mediaCodec, codecName, /* config= */ null, /* enableReuseVideoCodec= */ false);
    }

    TestableMediaCodecBridge(
        MediaCodec mediaCodec,
        String codecName,
        CodecConfig config,
        boolean enableReuseVideoCodec) {
      super(
          /* nativeMediaCodecBridge= */ 1000L,
          mediaCodec,
          codecName,
          TunnelModeAudioSessionId.NONE,
          /* enableFrameRendererListener= */ false,
          config,
          enableReuseVideoCodec);
    }

    @Override
    public void doRelease() {
      doReleaseCalled = true;
      super.doRelease();
    }
  }

  @Before
  public void setUp() {
    MediaCodecCache.resetCacheForTesting();
    mVisibility = new FakeSurfaceVisibility();
    MediaCodecCache.setSurfaceVisibilityForTesting(mVisibility);
    mMockSurface = mock(Surface.class);
    when(mMockSurface.isValid()).thenReturn(true);
    mMockMediaCodec = mock(MediaCodec.class);
    mMockNatives = mock(MediaCodecBridge.Natives.class);
    MediaCodecBridgeJni.setInstanceForTesting(mMockNatives);
    mBridge = new TestableMediaCodecBridge(mMockMediaCodec, "c2.mtk.av1.decoder");
  }

  @After
  public void tearDown() {
    MediaCodecBridgeJni.setInstanceForTesting(null);
    MediaCodecCache.resetCacheForTesting();
  }

  @Test
  public void testIsCompatible_SameMimeAndDecoder_ReturnsTrue() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    assertTrue(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_DifferentMime_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    // Requesting VP9 when AV1 is cached must return false.
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_VP9,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_DifferentDecoderName_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.android.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_DifferentSurface_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    Surface differentSurface = mock(Surface.class);
    when(differentSurface.isValid()).thenReturn(true);
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                differentSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_InvalidSurface_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    when(mMockSurface.isValid()).thenReturn(false);
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_LargerThanMaxResolution_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            1920,
            1080,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    // Requesting 4K when max resolution is 1080p must return false.
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                3840,
                2160,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_DifferentResolution_ReturnsFalse() {
    // Codec configured for 720x1280 (Shorts).
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            720,
            1280,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    // Requesting 1280x1280 (Square Shorts) must return false.
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1280,
                1280,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));

    // Requesting 3840x2160 (VoD) must return false.
    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                3840,
                2160,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_SameResolution_ReturnsTrue() {
    // Codec configured for 720x1280 (Shorts).
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            720,
            1280,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    // Consecutive Short of same resolution (720x1280) must return true.
    assertTrue(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                720,
                1280,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false)));
  }

  @Test
  public void testIsCompatible_CryptoOrHdrMismatch_ReturnsFalse() {
    CodecConfig cached =
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false);

    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ true,
                /* isHdr= */ false)));

    assertFalse(
        cached.isCompatible(
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                1920,
                1080,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ true)));
  }

  @Test
  public void testCreateVideoMediaCodecBridge_SameMime_ReusesCachedCodec() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false));
    MediaCodecCache.setCachedCodecForTesting(cached);

    CreateMediaCodecBridgeResult result = new CreateMediaCodecBridgeResult();
    MediaCodecBridge.createVideoMediaCodecBridge(
        /* nativeMediaCodecBridge= */ 2000L,
        MimeTypes.VIDEO_AV1,
        "c2.mtk.av1.decoder",
        1920,
        1080,
        /* fps= */ 60,
        3840,
        2160,
        mMockSurface,
        /* crypto= */ null,
        /* colorInfo= */ null,
        TunnelModeAudioSessionId.NONE,
        /* maxVideoInputSize= */ 0,
        /* enableFrameRendererListener= */ false,
        /* skipVideoFramesOver60Fps= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. Reuses the exact cached bridge instance.
    assertSame(mBridge, result.mediaCodecBridge());
    // 2. Notifies output format changed and starts the reused codec.
    verify(mMockNatives).onMediaCodecOutputFormatChanged(2000L);
    verify(mMockMediaCodec).start();
    // 3. Cache is now empty.
    assertNull(MediaCodecCache.getCachedCodecForTesting());
    // 4. The bridge was not released.
    assertFalse(mBridge.doReleaseCalled);
  }

  @Test
  public void
      testCreateVideoMediaCodecBridge_DifferentResolution_DestroysCachedCodecAndFallsBack() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                720,
                1280,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false));
    MediaCodecCache.setCachedCodecForTesting(cached);

    CreateMediaCodecBridgeResult result = new CreateMediaCodecBridgeResult();
    // Request with different resolution (VoD 3840x2160 instead of Shorts 720x1280) and decoder name
    // = "" to terminate fallback cleanly.
    MediaCodecBridge.createVideoMediaCodecBridge(
        /* nativeMediaCodecBridge= */ 2000L,
        MimeTypes.VIDEO_AV1,
        "",
        3840,
        2160,
        /* fps= */ 60,
        3840,
        2160,
        mMockSurface,
        /* crypto= */ null,
        /* colorInfo= */ null,
        TunnelModeAudioSessionId.NONE,
        /* maxVideoInputSize= */ 0,
        /* enableFrameRendererListener= */ false,
        /* skipVideoFramesOver60Fps= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. The incompatible cached codec was destroyed (doRelease called).
    assertTrue(mBridge.doReleaseCalled);
    // 2. Cached codec is removed.
    assertNull(MediaCodecCache.getCachedCodecForTesting());
    // 3. Decoder creation failed cleanly with invalid decoder name fallback.
    assertNull(result.mediaCodecBridge());
    assertEquals("Invalid decoder name.", result.errorMessage());
  }

  @Test
  public void testCreateVideoMediaCodecBridge_DifferentMime_DestroysCachedCodecAndFallsBack() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false));
    MediaCodecCache.setCachedCodecForTesting(cached);

    CreateMediaCodecBridgeResult result = new CreateMediaCodecBridgeResult();
    // Request with different MIME (VP9 instead of AV1) and decoder name = "" to terminate fallback
    // cleanly.
    MediaCodecBridge.createVideoMediaCodecBridge(
        /* nativeMediaCodecBridge= */ 2000L,
        MimeTypes.VIDEO_VP9,
        "",
        1920,
        1080,
        /* fps= */ 60,
        3840,
        2160,
        mMockSurface,
        /* crypto= */ null,
        /* colorInfo= */ null,
        TunnelModeAudioSessionId.NONE,
        /* maxVideoInputSize= */ 0,
        /* enableFrameRendererListener= */ false,
        /* skipVideoFramesOver60Fps= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. The incompatible cached codec was destroyed (doRelease called).
    assertTrue(mBridge.doReleaseCalled);
    // 2. Cached codec is removed.
    assertNull(MediaCodecCache.getCachedCodecForTesting());
    // 3. Decoder creation failed cleanly with invalid decoder name fallback.
    assertNull(result.mediaCodecBridge());
    assertEquals("Invalid decoder name.", result.errorMessage());
  }

  @Test
  public void testDiscardCachedCodec_DestroysCachedCodec() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
            new CodecConfig(
                MimeTypes.VIDEO_AV1,
                "c2.mtk.av1.decoder",
                mMockSurface,
                1920,
                1080,
                3840,
                2160,
                /* isTunneling= */ false,
                /* hasCrypto= */ false,
                /* isHdr= */ false));
    MediaCodecCache.setCachedCodecForTesting(cached);

    MediaCodecCache.discard();

    assertTrue(mBridge.doReleaseCalled);
    assertNull(MediaCodecCache.getCachedCodecForTesting());
  }

  @Test
  public void testRelease_CachingAllowed_CachesCodec() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.release();

    assertSame(bridge, MediaCodecCache.getCachedCodecForTesting().mBridge);
    assertFalse(bridge.doReleaseCalled);
    verify(mMockMediaCodec).flush();
  }

  @Test
  public void testStopCaching_ReleasesCachedCodec() {
    TestableMediaCodecBridge bridge = createReusableBridge();
    MediaCodecCache.setCachedCodecForTesting(new CachedCodec(bridge, bridge.getConfig()));

    MediaCodecCache.stopCaching();

    assertTrue(bridge.doReleaseCalled);
    assertNull(MediaCodecCache.getCachedCodecForTesting());
  }

  @Test
  public void testStopCaching_CodecInUse_ReleasedInsteadOfCachedOnRelease() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    // E.g., Activity.onStop() while a player still uses the codec.
    MediaCodecCache.stopCaching();

    // 1. The codec in use is not released by stopCaching().
    assertFalse(bridge.doReleaseCalled);

    // 2. When the player lets go of the codec, it is released instead of cached.
    bridge.release();
    assertTrue(bridge.doReleaseCalled);
    assertNull(MediaCodecCache.getCachedCodecForTesting());
  }

  @Test
  public void testAllowCaching_AfterStopCaching_CachesCodec() {
    TestableMediaCodecBridge bridge = createReusableBridge();
    MediaCodecCache.stopCaching();

    // E.g., a new video surface is created.
    MediaCodecCache.allowCaching();
    bridge.release();

    assertSame(bridge, MediaCodecCache.getCachedCodecForTesting().mBridge);
    assertFalse(bridge.doReleaseCalled);
  }

  @Test
  public void testStopCaching_DuringFlushOnRelease_ReleasedInsteadOfCached() {
    TestableMediaCodecBridge bridge = createReusableBridge();
    // Simulate stopCaching() racing with the flush that happens while the codec is being cached.
    doAnswer(
            invocation -> {
              MediaCodecCache.stopCaching();
              return null;
            })
        .when(mMockMediaCodec)
        .flush();

    bridge.release();

    assertTrue(bridge.doReleaseCalled);
    assertNull(MediaCodecCache.getCachedCodecForTesting());
  }

  @Test
  public void testFirstRenderedFrame_ShowsSurfaceWithThatFrameOnce() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.releaseOutputBufferAtTimestamp(/* index= */ 0, /* renderTimestampNs= */ 1000L);
    bridge.releaseOutputBufferAtTimestamp(/* index= */ 1, /* renderTimestampNs= */ 2000L);

    assertEquals(Arrays.asList("withNextFrame=true"), mVisibility.mCalls);
  }

  @Test
  public void testFirstFrameRenderedImmediately_ShowsSurfaceWithThatFrame() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.releaseOutputBuffer(/* index= */ 0, /* render= */ true);

    assertEquals(Arrays.asList("withNextFrame=true"), mVisibility.mCalls);
  }

  @Test
  public void testFrameReleasedWithoutRendering_DoesNotShowSurface() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.releaseOutputBuffer(/* index= */ 0, /* render= */ false);

    assertTrue(mVisibility.mCalls.isEmpty());
  }

  @Test
  public void testFirstRenderedFrame_ReuseDisabled_DoesNotChangeVisibility() {
    // mBridge is created with codec reuse disabled.
    mBridge.releaseOutputBufferAtTimestamp(/* index= */ 0, /* renderTimestampNs= */ 1000L);

    assertTrue(mVisibility.mCalls.isEmpty());
  }

  @Test
  public void testFirstRenderedFrame_CannotAttachToFrame_ShowsSurfaceImmediately() {
    mVisibility.mWithNextFrameSupported = false;
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.releaseOutputBufferAtTimestamp(/* index= */ 0, /* renderTimestampNs= */ 1000L);

    assertEquals(Arrays.asList("withNextFrame=true", "visible=true"), mVisibility.mCalls);
  }

  @Test
  public void testReusedCodec_HidesWhenCachedAndShowsWithNextFirstFrame() {
    TestableMediaCodecBridge bridge = createReusableBridge();
    bridge.releaseOutputBufferAtTimestamp(/* index= */ 0, /* renderTimestampNs= */ 1000L);

    // The playback ends: the codec is cached and the surface is hidden.
    bridge.release();
    assertSame(bridge, MediaCodecCache.getCachedCodecForTesting().mBridge);

    // The next playback reuses the codec and shows the surface with its first frame.
    CreateMediaCodecBridgeResult result = new CreateMediaCodecBridgeResult();
    MediaCodecBridge.createVideoMediaCodecBridge(
        /* nativeMediaCodecBridge= */ 2000L,
        MimeTypes.VIDEO_AV1,
        "c2.mtk.av1.decoder",
        1920,
        1080,
        /* fps= */ 60,
        3840,
        2160,
        mMockSurface,
        /* crypto= */ null,
        /* colorInfo= */ null,
        TunnelModeAudioSessionId.NONE,
        /* maxVideoInputSize= */ 0,
        /* enableFrameRendererListener= */ false,
        /* skipVideoFramesOver60Fps= */ false,
        /* enableReuseVideoCodec= */ true,
        result);
    assertSame(bridge, result.mediaCodecBridge());
    bridge.releaseOutputBufferAtTimestamp(/* index= */ 1, /* renderTimestampNs= */ 2000L);

    assertEquals(
        Arrays.asList("withNextFrame=true", "visible=false", "withNextFrame=true"),
        mVisibility.mCalls);
  }

  @Test
  public void testCodecReleasedBeforeFirstFrame_ShowsSurfaceImmediately() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    bridge.doRelease();

    assertEquals(Arrays.asList("visible=true"), mVisibility.mCalls);
  }

  @Test
  public void testCodecErrorBeforeFirstFrame_ShowsSurfaceImmediately() {
    TestableMediaCodecBridge bridge = createReusableBridge();

    MediaCodecCache.onError(bridge);

    assertEquals(Arrays.asList("visible=true"), mVisibility.mCalls);
  }

  @Test
  public void testDiscardCachedCodec_ShowsSurface() {
    TestableMediaCodecBridge bridge = createReusableBridge();
    bridge.releaseOutputBufferAtTimestamp(/* index= */ 0, /* renderTimestampNs= */ 1000L);
    bridge.release();

    MediaCodecCache.discard();

    assertTrue(bridge.doReleaseCalled);
    assertEquals(
        Arrays.asList("withNextFrame=true", "visible=false", "visible=true"), mVisibility.mCalls);
  }

  @Test
  public void testSetVideoSurfaceVisibleWithNextFrame_Unsupported_ReturnsFalse() {
    // These tests run below Android 14 and without a VideoSurfaceView.
    assertFalse(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
  }

  private TestableMediaCodecBridge createReusableBridge() {
    return new TestableMediaCodecBridge(
        mMockMediaCodec,
        "c2.mtk.av1.decoder",
        new CodecConfig(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            mMockSurface,
            1920,
            1080,
            3840,
            2160,
            /* isTunneling= */ false,
            /* hasCrypto= */ false,
            /* isHdr= */ false),
        /* enableReuseVideoCodec= */ true);
  }
}
