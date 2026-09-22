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

import static dev.cobalt.media.MediaCodecBridge.DECODER_FRAMEWORK_CODEC2;
import static dev.cobalt.media.MediaCodecBridge.DECODER_FRAMEWORK_OMX;
import static dev.cobalt.media.MediaCodecBridge.DECODER_FRAMEWORK_UNKNOWN;
import static org.junit.Assert.assertEquals;

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
        DECODER_FRAMEWORK_CODEC2, MediaCodecBridge.getDecoderFramework("c2.android.av1.decoder"));
    assertEquals(
        DECODER_FRAMEWORK_CODEC2,
        MediaCodecBridge.getDecoderFramework("c2.amlogic.av1.decoder.awesome"));
    assertEquals(
        DECODER_FRAMEWORK_CODEC2, MediaCodecBridge.getDecoderFramework("C2.qti.vp9.decoder"));
  }

  @Test
  public void testGetDecoderFramework_omxDecoders() {
    assertEquals(
        DECODER_FRAMEWORK_OMX,
        MediaCodecBridge.getDecoderFramework("OMX.amlogic.av1.decoder.awesome2"));
    assertEquals(
        DECODER_FRAMEWORK_OMX, MediaCodecBridge.getDecoderFramework("OMX.google.h264.decoder"));
    assertEquals(
        DECODER_FRAMEWORK_OMX, MediaCodecBridge.getDecoderFramework("omx.broadcom.video_decoder"));
  }

  @Test
  public void testGetDecoderFramework_unknownDecoders() {
    assertEquals(
        DECODER_FRAMEWORK_UNKNOWN, MediaCodecBridge.getDecoderFramework("unknown.video.decoder"));
    assertEquals(DECODER_FRAMEWORK_UNKNOWN, MediaCodecBridge.getDecoderFramework(""));
    assertEquals(DECODER_FRAMEWORK_UNKNOWN, MediaCodecBridge.getDecoderFramework(null));
  }
}
