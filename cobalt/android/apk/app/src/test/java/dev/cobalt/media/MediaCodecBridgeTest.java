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
import static org.junit.Assert.assertTrue;

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
 * <p>This test suite verifies the correctness of decoder framework detection logic (Codec2 vs
 * OpenMAX vs unknown) used for UMA telemetry.
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
    MediaFormat format = new MediaFormat();
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

    MediaCodecBridge.maybeConfigureHdrColorInfo(
        format,
        pqColorInfo,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);

    assertEquals(
        MediaFormat.COLOR_TRANSFER_ST2084, format.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
    assertEquals(
        MediaFormat.COLOR_STANDARD_BT2020, format.getInteger(MediaFormat.KEY_COLOR_STANDARD));
    assertEquals(MediaFormat.COLOR_RANGE_LIMITED, format.getInteger(MediaFormat.KEY_COLOR_RANGE));
    assertNotNull(format.getByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO));
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_hlgSetsColorWithoutHdrStaticInfo() {
    MediaFormat format = new MediaFormat();
    ColorInfo hlgColorInfo =
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
        hlgColorInfo,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);

    assertEquals(MediaFormat.COLOR_TRANSFER_HLG, format.getInteger(MediaFormat.KEY_COLOR_TRANSFER));
    assertEquals(
        MediaFormat.COLOR_STANDARD_BT2020, format.getInteger(MediaFormat.KEY_COLOR_STANDARD));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_RANGE));
    assertFalse(format.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));
  }

  @Test
  public void testMaybeConfigureHdrColorInfo_sdrTransferIgnoredEvenOnHdrDecoder() {
    MediaFormat format = new MediaFormat();
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

    MediaCodecBridge.maybeConfigureHdrColorInfo(
        format,
        fullRangeSdrColorInfo,
        /* isHdrCapableDecoder= */ true,
        /* restrictToTrueHdrTransfer= */ true);

    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_TRANSFER));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_STANDARD));
    assertFalse(format.containsKey(MediaFormat.KEY_COLOR_RANGE));
    assertFalse(format.containsKey(MediaFormat.KEY_HDR_STATIC_INFO));
    assertTrue(fullRangeSdrColorInfo.hdrStaticInfo != null);

    // When restrictToTrueHdrTransfer is false (default no-op path), legacy behavior is preserved.
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
}
