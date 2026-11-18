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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Handler;
import dev.cobalt.media.MediaCodecBridge.DecoderFramework;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

/**
 * Unit tests for {@link MediaCodecBridge}.
 *
 * <p>This test suite verifies the correctness of decoder framework detection logic (Codec2 vs
 * OpenMAX vs unknown) used for UMA telemetry, and the creation of an audio decoder in separate
 * steps (create, configure, attach and start) used by "Media.AsyncAudioCodecCreation".
 *
 * <p>Lifetime and ownership: Instantiated and owned by the JUnit test runner for the duration of
 * test execution.
 *
 * <p>Threading model: All tests execute sequentially on the main test runner thread.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecBridgeTest {
  private static final String AAC_MIME = "audio/mp4a-latm";
  private static final String OPUS_MIME = "audio/opus";
  private static final String AAC_DECODER_NAME = "c2.android.aac.decoder";
  private static final long NATIVE_BRIDGE = 1234L;

  private MediaCodecBridge.Natives mNatives;

  /** A MediaCodecBridge without native owner on top of a mock MediaCodec. */
  private static class DetachedCodec {
    final MediaCodec mMediaCodec = mock(MediaCodec.class);
    final MediaCodecBridge mBridge;
    final MediaCodec.Callback mCallback;

    DetachedCodec() {
      mBridge =
          new MediaCodecBridge(
              /* nativeMediaCodecBridge= */ 0,
              mMediaCodec,
              AAC_DECODER_NAME,
              TunnelModeAudioSessionId.NONE,
              /* enableFrameRendererListener= */ false);
      ArgumentCaptor<MediaCodec.Callback> callback =
          ArgumentCaptor.forClass(MediaCodec.Callback.class);
      verify(mMediaCodec).setCallback(callback.capture(), any(Handler.class));
      mCallback = callback.getValue();
    }
  }

  @Before
  public void setUp() {
    mNatives = mock(MediaCodecBridge.Natives.class);
    MediaCodecBridgeJni.setInstanceForTesting(mNatives);
  }

  @After
  public void tearDown() {
    MediaCodecBridgeJni.setInstanceForTesting(null);
  }

  private static MediaCodec.CodecException newCodecException() {
    return ReflectionHelpers.callConstructor(
        MediaCodec.CodecException.class,
        ClassParameter.from(int.class, 0),
        ClassParameter.from(int.class, 0),
        ClassParameter.from(String.class, "test"));
  }

  @Test
  public void createDetachedAudioDecoder_rejectsEmptyDecoderName() {
    assertNull(MediaCodecBridgeBuilder.createDetachedAudioDecoder(AAC_MIME, ""));
  }

  @Test
  public void createDetachedAudioDecoder_createsBridgeForNamedDecoder() {
    MediaCodecBridge bridge =
        MediaCodecBridgeBuilder.createDetachedAudioDecoder(AAC_MIME, AAC_DECODER_NAME);

    assertNotNull(bridge);
    assertEquals(AAC_DECODER_NAME, bridge.getCodecName());
    bridge.release();
  }

  @Test
  public void configureDetachedAudioDecoder_configuresAacWithoutCryptoAndDoesNotStart() {
    DetachedCodec codec = new DetachedCodec();

    assertTrue(
        codec.mBridge.configureDetachedAudioDecoder(
            AAC_MIME, 44100, 2, /* configurationData= */ null));

    ArgumentCaptor<MediaFormat> format = ArgumentCaptor.forClass(MediaFormat.class);
    verify(codec.mMediaCodec).configure(format.capture(), isNull(), isNull(), eq(0));
    assertEquals(AAC_MIME, format.getValue().getString(MediaFormat.KEY_MIME));
    assertEquals(44100, format.getValue().getInteger(MediaFormat.KEY_SAMPLE_RATE));
    assertEquals(2, format.getValue().getInteger(MediaFormat.KEY_CHANNEL_COUNT));
    assertEquals(1, format.getValue().getInteger(MediaFormat.KEY_IS_ADTS));
    // A codec that is not started delivers no buffer callbacks, which would be lost while the
    // bridge has no native owner.
    verify(codec.mMediaCodec, never()).start();
  }

  @Test
  public void configureDetachedAudioDecoder_returnsFalseWhenConfigureThrows() {
    DetachedCodec codec = new DetachedCodec();
    doThrow(new IllegalStateException("test"))
        .when(codec.mMediaCodec)
        .configure(any(), any(), any(), anyInt());

    assertFalse(
        codec.mBridge.configureDetachedAudioDecoder(
            AAC_MIME, 44100, 2, /* configurationData= */ null));
    verify(codec.mMediaCodec, never()).start();
  }

  @Test
  public void configureDetachedAudioDecoder_returnsFalseForOpusWithoutConfigurationData() {
    DetachedCodec codec = new DetachedCodec();

    assertFalse(
        codec.mBridge.configureDetachedAudioDecoder(
            OPUS_MIME, 48000, 2, /* configurationData= */ null));
    verify(codec.mMediaCodec, never()).configure(any(), any(), any(), anyInt());
  }

  @Test
  public void detachedCodec_dropsCallbacks() {
    DetachedCodec codec = new DetachedCodec();

    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 3);

    verify(mNatives, never()).onMediaCodecInputBufferAvailable(anyLong(), anyInt());
  }

  @Test
  public void attachAndStart_startsCodecAndDeliversCallbacksToNativeOwner() {
    DetachedCodec codec = new DetachedCodec();
    assertTrue(
        codec.mBridge.configureDetachedAudioDecoder(
            AAC_MIME, 44100, 2, /* configurationData= */ null));

    assertTrue(codec.mBridge.attachAndStart(NATIVE_BRIDGE));

    InOrder inOrder = inOrder(codec.mMediaCodec);
    inOrder.verify(codec.mMediaCodec).configure(any(), any(), any(), anyInt());
    inOrder.verify(codec.mMediaCodec).start();
    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 3);
    verify(mNatives).onMediaCodecInputBufferAvailable(NATIVE_BRIDGE, 3);
  }

  @Test
  public void attachAndStart_returnsFalseAndStaysDetachedWhenStartThrows() {
    DetachedCodec codec = new DetachedCodec();
    doThrow(new IllegalStateException("test")).when(codec.mMediaCodec).start();

    assertFalse(codec.mBridge.attachAndStart(NATIVE_BRIDGE));

    codec.mCallback.onInputBufferAvailable(codec.mMediaCodec, 3);
    verify(mNatives, never()).onMediaCodecInputBufferAvailable(anyLong(), anyInt());
    // The caller releases a bridge that could not be attached.
    codec.mBridge.release();
    verify(codec.mMediaCodec).release();
  }

  @Test
  public void attachAndStart_returnsFalseWithoutStartingAfterCodecError() {
    DetachedCodec codec = new DetachedCodec();
    codec.mCallback.onError(codec.mMediaCodec, newCodecException());

    assertFalse(codec.mBridge.attachAndStart(NATIVE_BRIDGE));

    verify(codec.mMediaCodec, never()).start();
    // The error happened while the bridge had no native owner, so it was not reported.
    verify(mNatives, never()).onMediaCodecError(anyLong(), anyBoolean(), anyBoolean(), any());
  }

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
}
