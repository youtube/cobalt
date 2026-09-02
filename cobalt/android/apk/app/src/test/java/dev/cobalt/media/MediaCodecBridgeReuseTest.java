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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.media.MediaCodec;
import android.view.Surface;
import dev.cobalt.media.MediaCodecBridge.CreateMediaCodecBridgeResult;
import dev.cobalt.media.MediaCodecBridge.MimeTypes;
import dev.cobalt.media.MediaCodecReuseCache.CachedCodec;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for MediaCodecBridge caching and reuse behavior across playbacks. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecBridgeReuseTest {

  private Surface mMockSurface;
  private MediaCodec mMockMediaCodec;
  private TestableMediaCodecBridge mBridge;

  private static class TestableMediaCodecBridge extends MediaCodecBridge {
    boolean doReleaseCalled = false;

    TestableMediaCodecBridge(MediaCodec mediaCodec, String codecName) {
      super(
          /* nativeMediaCodecBridge= */ 1000L,
          mediaCodec,
          codecName,
          TunnelModeAudioSessionId.NONE,
          /* enableFrameRendererListener= */ false,
          /* enableIgnoreCallbacksDuringFlushing= */ false);
    }

    @Override
    public void doRelease() {
      doReleaseCalled = true;
      super.doRelease();
    }
  }

  @Before
  public void setUp() {
    MediaCodecReuseCache.resetCacheForTesting();
    mMockSurface = mock(Surface.class);
    when(mMockSurface.isValid()).thenReturn(true);
    mMockMediaCodec = mock(MediaCodec.class);
    mBridge = new TestableMediaCodecBridge(mMockMediaCodec, "c2.mtk.av1.decoder");
  }

  @After
  public void tearDown() {
    MediaCodecReuseCache.resetCacheForTesting();
  }

  @Test
  public void testIsCompatible_SameMimeAndDecoder_ReturnsTrue() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_DifferentMime_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_VP9,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_DifferentDecoderName_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.android.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_DifferentSurface_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            differentSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_InvalidSurface_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_LargerThanMaxResolution_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            3840,
            2160,
            mMockSurface,
            3840,
            2160,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_DifferentResolution_ReturnsFalse() {
    // Codec configured for 720x1280 (Shorts).
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1280,
            1280,
            mMockSurface,
            3840,
            2160,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));

    // Requesting 3840x2160 (VoD) must return false.
    assertFalse(
        cached.isCompatible(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            3840,
            2160,
            mMockSurface,
            3840,
            2160,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_SameResolution_ReturnsTrue() {
    // Codec configured for 720x1280 (Shorts).
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            720,
            1280,
            mMockSurface,
            3840,
            2160,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ false));
  }

  @Test
  public void testIsCompatible_CryptoOrHdrMismatch_ReturnsFalse() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ true,
            /* reqHdr= */ false));

    assertFalse(
        cached.isCompatible(
            MimeTypes.VIDEO_AV1,
            "c2.mtk.av1.decoder",
            1920,
            1080,
            mMockSurface,
            1920,
            1080,
            /* reqTunneling= */ false,
            /* reqCrypto= */ false,
            /* reqHdr= */ true));
  }

  @Test
  public void testCreateVideoMediaCodecBridge_SameMime_ReusesCachedCodec() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
    MediaCodecReuseCache.setCachedCodecForTesting(cached);

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
        /* ignoreCodecCallbacksDuringFlushing= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. Reuses the exact cached bridge instance.
    assertSame(mBridge, result.mediaCodecBridge());
    // 2. Starts the reused codec.
    verify(mMockMediaCodec).start();
    // 3. Cache is now empty.
    assertNull(MediaCodecReuseCache.getCachedCodecForTesting());
    // 4. The bridge was not released.
    assertFalse(mBridge.doReleaseCalled);
  }

  @Test
  public void
      testCreateVideoMediaCodecBridge_DifferentResolution_DestroysCachedCodecAndFallsBack() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
    MediaCodecReuseCache.setCachedCodecForTesting(cached);

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
        /* ignoreCodecCallbacksDuringFlushing= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. The incompatible cached codec was destroyed (doRelease called).
    assertTrue(mBridge.doReleaseCalled);
    // 2. Cached codec is removed.
    assertNull(MediaCodecReuseCache.getCachedCodecForTesting());
    // 3. Decoder creation failed cleanly with invalid decoder name fallback.
    assertNull(result.mediaCodecBridge());
    assertEquals("Invalid decoder name.", result.errorMessage());
  }

  @Test
  public void testCreateVideoMediaCodecBridge_DifferentMime_DestroysCachedCodecAndFallsBack() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
    MediaCodecReuseCache.setCachedCodecForTesting(cached);

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
        /* ignoreCodecCallbacksDuringFlushing= */ false,
        /* enableReuseVideoCodec= */ true,
        result);

    // 1. The incompatible cached codec was destroyed (doRelease called).
    assertTrue(mBridge.doReleaseCalled);
    // 2. Cached codec is removed.
    assertNull(MediaCodecReuseCache.getCachedCodecForTesting());
    // 3. Decoder creation failed cleanly with invalid decoder name fallback.
    assertNull(result.mediaCodecBridge());
    assertEquals("Invalid decoder name.", result.errorMessage());
  }

  @Test
  public void testDiscardCachedCodec_DestroysCachedCodec() {
    CachedCodec cached =
        new CachedCodec(
            mBridge,
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
    MediaCodecReuseCache.setCachedCodecForTesting(cached);

    MediaCodecReuseCache.discard();

    assertTrue(mBridge.doReleaseCalled);
    assertNull(MediaCodecReuseCache.getCachedCodecForTesting());
  }
}
