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
import static org.junit.Assert.assertNotNull;

import android.media.MediaFormat;
import dev.cobalt.media.MediaCodecBridge.ColorInfo;
import dev.cobalt.media.MediaCodecBridge.DecoderFramework;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/**
 * Unit tests for {@link MediaCodecBridge}.
 *
 * <p>This test suite verifies decoder framework detection logic used for UMA telemetry and HDR
 * {@link MediaFormat} configuration in {@link MediaCodecBridge#maybeConfigureHdrColorInfo}.
 *
 * <p>Lifetime and ownership: Instantiated and owned by the JUnit test runner for the duration of
 * test execution.
 *
 * <p>Threading model: All tests execute sequentially on the main test runner thread.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecBridgeTest {

  @Test
  public void testGetDecoderFramework_c2Decoders() {
    assertEquals(
        DecoderFramework.CODEC2, MediaCodecBridge.getDecoderFramework("c2.android.av1.decoder"));
    assertEquals(
        DecoderFramework.CODEC2,
        MediaCodecBridge.getDecoderFramework("c2.amlogic.av1.decoder.awesome"));
    assertEquals(
        DecoderFramework.CODEC2, MediaCodecBridge.getDecoderFramework("C2.qti.vp9.decoder"));
  }

  @Test
  public void testGetDecoderFramework_omxDecoders() {
    assertEquals(
        DecoderFramework.OMX,
        MediaCodecBridge.getDecoderFramework("OMX.amlogic.av1.decoder.awesome2"));
    assertEquals(
        DecoderFramework.OMX, MediaCodecBridge.getDecoderFramework("OMX.google.h264.decoder"));
    assertEquals(
        DecoderFramework.OMX, MediaCodecBridge.getDecoderFramework("omx.broadcom.video_decoder"));
  }

  @Test
  public void testGetDecoderFramework_unknownDecoders() {
    assertEquals(
        DecoderFramework.UNKNOWN, MediaCodecBridge.getDecoderFramework("unknown.video.decoder"));
    assertEquals(DecoderFramework.UNKNOWN, MediaCodecBridge.getDecoderFramework(""));
    assertEquals(DecoderFramework.UNKNOWN, MediaCodecBridge.getDecoderFramework(null));
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_pqSetsColorAndHdrStaticInfo() {
    ColorInfo pqColorInfo =
        new ColorInfo(
            MediaFormat.COLOR_RANGE_LIMITED,
            MediaFormat.COLOR_STANDARD_BT2020,
            MediaFormat.COLOR_TRANSFER_ST2084,
            0.708f,
            0.292f,
            0.170f,
            0.797f,
            0.131f,
            0.046f,
            0.3127f,
            0.3290f,
            1000.0f,
            0.005f,
            1000,
            400);

    for (boolean restrictToTrueHdrTransfer : new boolean[] {false, true}) {
      MediaFormat format = new MediaFormat();
      MediaCodecBridge.maybeConfigureHdrColorInfo(
          format, pqColorInfo, /* isHdrCapableDecoder= */ true, restrictToTrueHdrTransfer);

      assertEquals(
          MediaFormat.COLOR_TRANSFER_ST2084, format.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
      assertEquals(
          MediaFormat.COLOR_STANDARD_BT2020, format.getInteger(MediaFormat.KEY_COLOR_STANDARD));
      assertEquals(MediaFormat.COLOR_RANGE_LIMITED, format.getInteger(MediaFormat.KEY_COLOR_RANGE));
      assertNotNull(format.getByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO));
    }
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_hlgSetsColorWithoutHdrStaticInfo() {
    MediaFormat format = new MediaFormat();
    ColorInfo hlgUnspecifiedRange =
        new ColorInfo(
            /* colorRange= */ 0,
            MediaFormat.COLOR_STANDARD_BT2020,
            MediaFormat.COLOR_TRANSFER_HLG,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0,
            0);

    MediaCodecBridge.maybeConfigureHdrColorInfo(
        format,
        hlgUnspecifiedRange,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);

    assertEquals(MediaFormat.COLOR_TRANSFER_HLG, format.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
    assertEquals(
        MediaFormat.COLOR_STANDARD_BT2020, format.getInteger(MediaFormat.KEY_COLOR_STANDARD));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_RANGE));
    assertFalse(format.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));

    MediaFormat limitedRangeHlgFormat = new MediaFormat();
    ColorInfo hlgLimitedRange =
        new ColorInfo(
            MediaFormat.COLOR_RANGE_LIMITED,
            MediaFormat.COLOR_STANDARD_BT2020,
            MediaFormat.COLOR_TRANSFER_HLG,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0,
            0);
    MediaCodecBridge.maybeConfigureHdrColorInfo(
        limitedRangeHlgFormat,
        hlgLimitedRange,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);
    assertEquals(
        MediaFormat.COLOR_RANGE_LIMITED,
        limitedRangeHlgFormat.getInteger(MediaFormat.KEY_COLOR_RANGE));
    assertFalse(limitedRangeHlgFormat.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));

    // Legacy unflagged path attaches KEY_HDR_STATIC_INFO to HLG as well.
    MediaFormat legacyFormat = new MediaFormat();
    MediaCodecBridge.maybeConfigureHdrColorInfo(
        legacyFormat,
        hlgUnspecifiedRange,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ false);
    assertEquals(
        MediaFormat.COLOR_TRANSFER_HLG, legacyFormat.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
    assertNotNull(legacyFormat.getByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO));
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_sdrTransferIgnoredEvenOnHdrDecoder() {
    ColorInfo fullRangeSdrColorInfo =
        new ColorInfo(
            MediaFormat.COLOR_RANGE_FULL,
            MediaFormat.COLOR_STANDARD_BT709,
            MediaFormat.COLOR_TRANSFER_SDR_VIDEO,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0,
            0);
    assertNotNull(fullRangeSdrColorInfo.hdrStaticInfo);

    MediaFormat format = new MediaFormat();
    MediaCodecBridge.maybeConfigureHdrColorInfo(
        format,
        fullRangeSdrColorInfo,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);

    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_TRANSFER));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_STANDARD));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_RANGE));
    assertFalse(format.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));

    // When restrictToTrueHdrTransfer is false (default path), legacy behavior is preserved.
    MediaFormat legacyFormat = new MediaFormat();
    MediaCodecBridge.maybeConfigureHdrColorInfo(
        legacyFormat,
        fullRangeSdrColorInfo,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ false);
    assertEquals(
        MediaFormat.COLOR_TRANSFER_SDR_VIDEO,
        legacyFormat.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
    assertNotNull(legacyFormat.getByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO));
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_nullColorInfoOrNonHdrDecoderSetsNothing() {
    ColorInfo pqColorInfo =
        new ColorInfo(
            MediaFormat.COLOR_RANGE_LIMITED,
            MediaFormat.COLOR_STANDARD_BT2020,
            MediaFormat.COLOR_TRANSFER_ST2084,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            0f,
            1000.0f,
            0.005f,
            1000,
            400);

    for (boolean restrictToTrueHdrTransfer : new boolean[] {false, true}) {
      // Null ColorInfo (such as BT.709 limited SDR, or BT.601 SDR whose Smpte170M primaries map to
      // COLOR_VALUE_UNKNOWN in native CreateVideoMediaCodec) sets no HDR keys.
      MediaFormat nullColorFormat = new MediaFormat();
      MediaCodecBridge.maybeConfigureHdrColorInfo(
          nullColorFormat,
          /* colorInfo= */ null,
          /* isHdrCapableDecoder= */ true,
          restrictToTrueHdrTransfer);
      assertFalse(nullColorFormat.containsKey(MediaFormat.KEY_COLOR_TRANSFER));
      assertFalse(nullColorFormat.containsKey(MediaFormat.KEY_COLOR_STANDARD));
      assertFalse(nullColorFormat.containsKey(MediaFormat.KEY_COLOR_RANGE));
      assertFalse(nullColorFormat.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));

      // Non-HDR-capable decoder sets no HDR keys even when ColorInfo is PQ.
      MediaFormat nonHdrDecoderFormat = new MediaFormat();
      MediaCodecBridge.maybeConfigureHdrColorInfo(
          nonHdrDecoderFormat,
          pqColorInfo,
          /* isHdrCapableDecoder= */ false,
          restrictToTrueHdrTransfer);
      assertFalse(nonHdrDecoderFormat.containsKey(MediaFormat.KEY_COLOR_TRANSFER));
      assertFalse(nonHdrDecoderFormat.containsKey(MediaFormat.KEY_COLOR_STANDARD));
      assertFalse(nonHdrDecoderFormat.containsKey(MediaFormat.KEY_COLOR_RANGE));
      assertFalse(nonHdrDecoderFormat.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));
    }
  }
}
