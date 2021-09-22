// Copyright 2013 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import static dev.cobalt.media.Log.TAG;

import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.view.Surface;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** A wrapper of the MediaCodec class. */
@SuppressWarnings("unused")
@UsedByNative
class MediaCodecBridge {
  // Error code for MediaCodecBridge. Keep this value in sync with
  // MEDIA_CODEC_* values in media_codec_bridge.h.
  private static final int MEDIA_CODEC_OK = 0;
  private static final int MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER = 1;
  private static final int MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER = 2;
  private static final int MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED = 3;
  private static final int MEDIA_CODEC_OUTPUT_FORMAT_CHANGED = 4;
  private static final int MEDIA_CODEC_INPUT_END_OF_STREAM = 5;
  private static final int MEDIA_CODEC_OUTPUT_END_OF_STREAM = 6;
  private static final int MEDIA_CODEC_NO_KEY = 7;
  private static final int MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION = 8;
  private static final int MEDIA_CODEC_ABORT = 9;
  private static final int MEDIA_CODEC_ERROR = 10;

  // After a flush(), dequeueOutputBuffer() can often produce empty presentation timestamps
  // for several frames. As a result, the player may find that the time does not increase
  // after decoding a frame. To detect this, we check whether the presentation timestamp from
  // dequeueOutputBuffer() is larger than input_timestamp - MAX_PRESENTATION_TIMESTAMP_SHIFT_US
  // after a flush. And we set the presentation timestamp from dequeueOutputBuffer() to be
  // non-decreasing for the remaining frames.
  private static final long MAX_PRESENTATION_TIMESTAMP_SHIFT_US = 100000;

  // We use only one output audio format (PCM16) that has 2 bytes per sample
  private static final int PCM16_BYTES_PER_SAMPLE = 2;

  // TODO: Use MediaFormat constants when part of the public API.
  private static final String KEY_CROP_LEFT = "crop-left";
  private static final String KEY_CROP_RIGHT = "crop-right";
  private static final String KEY_CROP_BOTTOM = "crop-bottom";
  private static final String KEY_CROP_TOP = "crop-top";

  private static final int BITRATE_ADJUSTMENT_FPS = 30;

  private long mNativeMediaCodecBridge;
  private MediaCodec mMediaCodec;
  private MediaCodec.Callback mCallback;
  private boolean mFlushed;
  private long mLastPresentationTimeUs;
  private final String mMime;
  private boolean mAdaptivePlaybackSupported;
  private double mPlaybackRate = 1.0;
  private int mFps = 30;

  private MediaCodec.OnFrameRenderedListener mTunnelModeFrameRendererListener;

  // Functions that require this will be called frequently in a tight loop.
  // Only create one of these and reuse it to avoid excessive allocations,
  // which would cause GC cycles long enough to impact playback.
  private final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

  // Type of bitrate adjustment for video encoder.
  public enum BitrateAdjustmentTypes {
    // No adjustment - video encoder has no known bitrate problem.
    NO_ADJUSTMENT,
    // Framerate based bitrate adjustment is required - HW encoder does not use frame
    // timestamps to calculate frame bitrate budget and instead is relying on initial
    // fps configuration assuming that all frames are coming at fixed initial frame rate.
    FRAMERATE_ADJUSTMENT,
  }

  public static final class MimeTypes {
    public static final String VIDEO_MP4 = "video/mp4";
    public static final String VIDEO_WEBM = "video/webm";
    public static final String VIDEO_H264 = "video/avc";
    public static final String VIDEO_H265 = "video/hevc";
    public static final String VIDEO_VP8 = "video/x-vnd.on2.vp8";
    public static final String VIDEO_VP9 = "video/x-vnd.on2.vp9";
    public static final String VIDEO_AV1 = "video/av01";
  }

  private class FrameRateEstimator {
    private static final int INVALID_FRAME_RATE = -1;
    private static final long INVALID_FRAME_TIMESTAMP = -1;
    private static final int MINIMUM_REQUIRED_FRAMES = 4;
    private long mLastFrameTimestampUs = INVALID_FRAME_TIMESTAMP;
    private long mNumberOfFrames = 0;
    private long mTotalDurationUs = 0;

    public int getEstimatedFrameRate() {
      if (mTotalDurationUs <= 0 || mNumberOfFrames < MINIMUM_REQUIRED_FRAMES) {
        return INVALID_FRAME_RATE;
      }
      return Math.round((mNumberOfFrames - 1) * 1000000.0f / mTotalDurationUs);
    }

    public void reset() {
      mLastFrameTimestampUs = INVALID_FRAME_TIMESTAMP;
      mNumberOfFrames = 0;
      mTotalDurationUs = 0;
    }

    public void onNewFrame(long presentationTimeUs) {
      mNumberOfFrames++;

      if (mLastFrameTimestampUs == INVALID_FRAME_TIMESTAMP) {
        mLastFrameTimestampUs = presentationTimeUs;
        return;
      }
      if (presentationTimeUs <= mLastFrameTimestampUs) {
        Log.v(TAG, String.format("Invalid output presentation timestamp."));
        return;
      }

      mTotalDurationUs += presentationTimeUs - mLastFrameTimestampUs;
      mLastFrameTimestampUs = presentationTimeUs;
    }
  }

  private FrameRateEstimator mFrameRateEstimator = null;
  private BitrateAdjustmentTypes mBitrateAdjustmentType = BitrateAdjustmentTypes.NO_ADJUSTMENT;

  @SuppressWarnings("unused")
  @UsedByNative
  public static class DequeueInputResult {
    private int mStatus;
    private int mIndex;

    @SuppressWarnings("unused")
    @UsedByNative
    private DequeueInputResult() {
      mStatus = MEDIA_CODEC_ERROR;
      mIndex = -1;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private DequeueInputResult(int status, int index) {
      mStatus = status;
      mIndex = index;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int status() {
      return mStatus;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int index() {
      return mIndex;
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private static class DequeueOutputResult {
    private int mStatus;
    private int mIndex;
    private int mFlags;
    private int mOffset;
    private long mPresentationTimeMicroseconds;
    private int mNumBytes;

    @SuppressWarnings("unused")
    @UsedByNative
    private DequeueOutputResult() {
      mStatus = MEDIA_CODEC_ERROR;
      mIndex = -1;
      mFlags = 0;
      mOffset = 0;
      mPresentationTimeMicroseconds = 0;
      mNumBytes = 0;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private DequeueOutputResult(
        int status,
        int index,
        int flags,
        int offset,
        long presentationTimeMicroseconds,
        int numBytes) {
      mStatus = status;
      mIndex = index;
      mFlags = flags;
      mOffset = offset;
      mPresentationTimeMicroseconds = presentationTimeMicroseconds;
      mNumBytes = numBytes;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int status() {
      return mStatus;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int index() {
      return mIndex;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int flags() {
      return mFlags;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int offset() {
      return mOffset;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private long presentationTimeMicroseconds() {
      return mPresentationTimeMicroseconds;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int numBytes() {
      return mNumBytes;
    }
  }

  /** A wrapper around a MediaFormat. */
  @SuppressWarnings("unused")
  @UsedByNative
  private static class GetOutputFormatResult {
    private int mStatus;
    // May be null if mStatus is not MEDIA_CODEC_OK.
    private MediaFormat mFormat;

    @SuppressWarnings("unused")
    @UsedByNative
    private GetOutputFormatResult() {
      mStatus = MEDIA_CODEC_ERROR;
      mFormat = null;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private GetOutputFormatResult(int status, MediaFormat format) {
      mStatus = status;
      mFormat = format;
    }

    private boolean formatHasCropValues() {
      return mFormat.containsKey(KEY_CROP_RIGHT)
          && mFormat.containsKey(KEY_CROP_LEFT)
          && mFormat.containsKey(KEY_CROP_BOTTOM)
          && mFormat.containsKey(KEY_CROP_TOP);
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int status() {
      return mStatus;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int width() {
      return formatHasCropValues()
          ? mFormat.getInteger(KEY_CROP_RIGHT) - mFormat.getInteger(KEY_CROP_LEFT) + 1
          : mFormat.getInteger(MediaFormat.KEY_WIDTH);
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int height() {
      return formatHasCropValues()
          ? mFormat.getInteger(KEY_CROP_BOTTOM) - mFormat.getInteger(KEY_CROP_TOP) + 1
          : mFormat.getInteger(MediaFormat.KEY_HEIGHT);
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int sampleRate() {
      return mFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int channelCount() {
      return mFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private static class ColorInfo {
    private static final int MAX_CHROMATICITY = 50000; // Defined in CTA-861.3.
    private static final int DEFAULT_MAX_CLL = 1000;
    private static final int DEFAULT_MAX_FALL = 200;

    public int colorRange;
    public int colorStandard;
    public int colorTransfer;
    public ByteBuffer hdrStaticInfo;

    @SuppressWarnings("unused")
    @UsedByNative
    ColorInfo(
        int colorRange,
        int colorStandard,
        int colorTransfer,
        float primaryRChromaticityX,
        float primaryRChromaticityY,
        float primaryGChromaticityX,
        float primaryGChromaticityY,
        float primaryBChromaticityX,
        float primaryBChromaticityY,
        float whitePointChromaticityX,
        float whitePointChromaticityY,
        float maxMasteringLuminance,
        float minMasteringLuminance,
        int maxCll,
        int maxFall,
        boolean forceBigEndianHdrMetadata) {
      this.colorRange = colorRange;
      this.colorStandard = colorStandard;
      this.colorTransfer = colorTransfer;

      if (maxCll <= 0) {
        maxCll = DEFAULT_MAX_CLL;
      }
      if (maxFall <= 0) {
        maxFall = DEFAULT_MAX_FALL;
      }

      // This logic is inspired by
      // https://cs.android.com/android/_/android/platform/external/exoplayer/+/3423b4bbfffbb62b5f2d8f16cfdc984dc107cd02:tree/library/extractor/src/main/java/com/google/android/exoplayer2/extractor/mkv/MatroskaExtractor.java;l=2200-2215;drc=9af07bc62f8115cbaa6f1178ce8aa3533d2b9e29.
      ByteBuffer hdrStaticInfo = ByteBuffer.allocateDirect(25);
      // Force big endian in case the HDR metadata causes problems in production.
      if (forceBigEndianHdrMetadata) {
        hdrStaticInfo.order(ByteOrder.BIG_ENDIAN);
      } else {
        hdrStaticInfo.order(ByteOrder.LITTLE_ENDIAN);
      }

      hdrStaticInfo.put((byte) 0);
      hdrStaticInfo.putShort((short) ((primaryRChromaticityX * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((primaryRChromaticityY * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((primaryGChromaticityX * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((primaryGChromaticityY * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((primaryBChromaticityX * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((primaryBChromaticityY * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((whitePointChromaticityX * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) ((whitePointChromaticityY * MAX_CHROMATICITY) + 0.5f));
      hdrStaticInfo.putShort((short) (maxMasteringLuminance + 0.5f));
      hdrStaticInfo.putShort((short) (minMasteringLuminance + 0.5f));
      hdrStaticInfo.putShort((short) maxCll);
      hdrStaticInfo.putShort((short) maxFall);
      hdrStaticInfo.rewind();
      this.hdrStaticInfo = hdrStaticInfo;
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private static class CreateMediaCodecBridgeResult {
    private MediaCodecBridge mMediaCodecBridge;
    // Contains the error message when mMediaCodecBridge is null.
    private String mErrorMessage;

    @SuppressWarnings("unused")
    @UsedByNative
    private CreateMediaCodecBridgeResult() {
      mMediaCodecBridge = null;
      mErrorMessage = "";
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private MediaCodecBridge mediaCodecBridge() {
      return mMediaCodecBridge;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private String errorMessage() {
      return mErrorMessage;
    }
  }

  private MediaCodecBridge(
      long nativeMediaCodecBridge,
      MediaCodec mediaCodec,
      String mime,
      boolean adaptivePlaybackSupported,
      BitrateAdjustmentTypes bitrateAdjustmentType,
      int tunnelModeAudioSessionId) {
    if (mediaCodec == null) {
      throw new IllegalArgumentException();
    }
    mNativeMediaCodecBridge = nativeMediaCodecBridge;
    mMediaCodec = mediaCodec;
    mMime = mime; // TODO: Delete the unused mMime field
    mLastPresentationTimeUs = 0;
    mFlushed = true;
    mAdaptivePlaybackSupported = adaptivePlaybackSupported;
    mBitrateAdjustmentType = bitrateAdjustmentType;
    mCallback =
        new MediaCodec.Callback() {
          @Override
          public void onError(MediaCodec codec, MediaCodec.CodecException e) {
            synchronized (this) {
              if (mNativeMediaCodecBridge == 0) {
                return;
              }
              nativeOnMediaCodecError(
                  mNativeMediaCodecBridge,
                  e.isRecoverable(),
                  e.isTransient(),
                  e.getDiagnosticInfo());
            }
          }

          @Override
          public void onInputBufferAvailable(MediaCodec codec, int index) {
            synchronized (this) {
              if (mNativeMediaCodecBridge == 0) {
                return;
              }
              nativeOnMediaCodecInputBufferAvailable(mNativeMediaCodecBridge, index);
            }
          }

          @Override
          public void onOutputBufferAvailable(
              MediaCodec codec, int index, MediaCodec.BufferInfo info) {
            synchronized (this) {
              if (mNativeMediaCodecBridge == 0) {
                return;
              }
              nativeOnMediaCodecOutputBufferAvailable(
                  mNativeMediaCodecBridge,
                  index,
                  info.flags,
                  info.offset,
                  info.presentationTimeUs,
                  info.size);
              if (mFrameRateEstimator != null) {
                mFrameRateEstimator.onNewFrame(info.presentationTimeUs);
                int fps = mFrameRateEstimator.getEstimatedFrameRate();
                if (fps != FrameRateEstimator.INVALID_FRAME_RATE && mFps != fps) {
                  mFps = fps;
                  updateOperatingRate();
                }
              }
            }
          }

          @Override
          public void onOutputFormatChanged(MediaCodec codec, MediaFormat format) {
            synchronized (this) {
              if (mNativeMediaCodecBridge == 0) {
                return;
              }
              nativeOnMediaCodecOutputFormatChanged(mNativeMediaCodecBridge);
            }
          }
        };
    mMediaCodec.setCallback(mCallback);

    // TODO: support OnFrameRenderedListener for non tunnel mode
    if (tunnelModeAudioSessionId != -1 && Build.VERSION.SDK_INT >= 23) {
      mTunnelModeFrameRendererListener =
          new MediaCodec.OnFrameRenderedListener() {
            @Override
            public void onFrameRendered(MediaCodec codec, long presentationTimeUs, long nanoTime) {
              synchronized (this) {
                if (mNativeMediaCodecBridge == 0) {
                  return;
                }
                nativeOnMediaCodecFrameRendered(
                    mNativeMediaCodecBridge, presentationTimeUs, nanoTime);
              }
            }
          };
      mMediaCodec.setOnFrameRenderedListener(mTunnelModeFrameRendererListener, null);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public static MediaCodecBridge createAudioMediaCodecBridge(
      long nativeMediaCodecBridge,
      String mime,
      boolean isSecure,
      boolean requireSoftwareCodec,
      int sampleRate,
      int channelCount,
      MediaCrypto crypto) {
    MediaCodec mediaCodec = null;
    try {
      String decoderName = MediaCodecUtil.findAudioDecoder(mime, 0, false);
      if (decoderName.equals("")) {
        Log.e(TAG, String.format("Failed to find decoder: %s, isSecure: %s", mime, isSecure));
        return null;
      }
      Log.i(TAG, String.format("Creating \"%s\" decoder.", decoderName));
      mediaCodec = MediaCodec.createByCodecName(decoderName);
    } catch (Exception e) {
      Log.e(TAG, String.format("Failed to create MediaCodec: %s, isSecure: %s", mime, isSecure), e);
      return null;
    }
    if (mediaCodec == null) {
      return null;
    }
    MediaCodecBridge bridge =
        new MediaCodecBridge(
            nativeMediaCodecBridge,
            mediaCodec,
            mime,
            true,
            BitrateAdjustmentTypes.NO_ADJUSTMENT,
            -1);

    MediaFormat mediaFormat = createAudioFormat(mime, sampleRate, channelCount);
    setFrameHasADTSHeader(mediaFormat);
    if (!bridge.configureAudio(mediaFormat, crypto, 0)) {
      Log.e(TAG, "Failed to configure audio codec.");
      bridge.release();
      return null;
    }
    if (!bridge.start()) {
      Log.e(TAG, "Failed to start audio codec.");
      bridge.release();
      return null;
    }

    return bridge;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public static void createVideoMediaCodecBridge(
      long nativeMediaCodecBridge,
      String mime,
      boolean isSecure,
      boolean requireSoftwareCodec,
      int width,
      int height,
      int fps,
      Surface surface,
      MediaCrypto crypto,
      ColorInfo colorInfo,
      int tunnelModeAudioSessionId,
      CreateMediaCodecBridgeResult outCreateMediaCodecBridgeResult) {
    MediaCodec mediaCodec = null;
    outCreateMediaCodecBridgeResult.mMediaCodecBridge = null;

    boolean findHDRDecoder = android.os.Build.VERSION.SDK_INT >= 24 && colorInfo != null;
    boolean findTunneledDecoder = tunnelModeAudioSessionId != -1;
    // On first pass, try to find a decoder with HDR if the color info is non-null.
    MediaCodecUtil.FindVideoDecoderResult findVideoDecoderResult =
        MediaCodecUtil.findVideoDecoder(
            mime, isSecure, 0, 0, 0, 0, findHDRDecoder, requireSoftwareCodec, findTunneledDecoder);
    if (findVideoDecoderResult.name.equals("") && findHDRDecoder) {
      // On second pass, forget HDR.
      findVideoDecoderResult =
          MediaCodecUtil.findVideoDecoder(
              mime, isSecure, 0, 0, 0, 0, false, requireSoftwareCodec, findTunneledDecoder);
    }
    try {
      String decoderName = findVideoDecoderResult.name;
      if (decoderName.equals("") || findVideoDecoderResult.videoCapabilities == null) {
        Log.e(TAG, String.format("Failed to find decoder: %s, isSecure: %s", mime, isSecure));
        outCreateMediaCodecBridgeResult.mErrorMessage =
            String.format("Failed to find decoder: %s, isSecure: %s", mime, isSecure);
        return;
      }
      Log.i(TAG, String.format("Creating \"%s\" decoder.", decoderName));
      mediaCodec = MediaCodec.createByCodecName(decoderName);
    } catch (Exception e) {
      Log.e(TAG, String.format("Failed to create MediaCodec: %s, isSecure: %s", mime, isSecure), e);
      outCreateMediaCodecBridgeResult.mErrorMessage =
          String.format("Failed to create MediaCodec: %s, isSecure: %s", mime, isSecure);
      return;
    }
    if (mediaCodec == null) {
      outCreateMediaCodecBridgeResult.mErrorMessage = "mediaCodec is null";
      return;
    }
    MediaCodecBridge bridge =
        new MediaCodecBridge(
            nativeMediaCodecBridge,
            mediaCodec,
            mime,
            true,
            BitrateAdjustmentTypes.NO_ADJUSTMENT,
            tunnelModeAudioSessionId);
    MediaFormat mediaFormat =
        createVideoDecoderFormat(mime, width, height, findVideoDecoderResult.videoCapabilities);

    boolean shouldConfigureHdr =
        android.os.Build.VERSION.SDK_INT >= 24
            && colorInfo != null
            && MediaCodecUtil.isHdrCapableVideoDecoder(mime, findVideoDecoderResult);
    if (shouldConfigureHdr) {
      Log.d(TAG, "Setting HDR info.");
      mediaFormat.setInteger(MediaFormat.KEY_COLOR_TRANSFER, colorInfo.colorTransfer);
      mediaFormat.setInteger(MediaFormat.KEY_COLOR_STANDARD, colorInfo.colorStandard);
      // If color range is unspecified, don't set it.
      if (colorInfo.colorRange != 0) {
        mediaFormat.setInteger(MediaFormat.KEY_COLOR_RANGE, colorInfo.colorRange);
      }
      mediaFormat.setByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO, colorInfo.hdrStaticInfo);
    }

    if (tunnelModeAudioSessionId != -1) {
      mediaFormat.setFeatureEnabled(CodecCapabilities.FEATURE_TunneledPlayback, true);
      mediaFormat.setInteger(MediaFormat.KEY_AUDIO_SESSION_ID, tunnelModeAudioSessionId);
      Log.d(
          TAG,
          String.format(
              "Enabled tunnel mode playback on audio session %d", tunnelModeAudioSessionId));
    }

    VideoCapabilities videoCapabilities = findVideoDecoderResult.videoCapabilities;
    int maxWidth = videoCapabilities.getSupportedWidths().getUpper();
    int maxHeight = videoCapabilities.getSupportedHeights().getUpper();
    if (fps > 0) {
      if (!videoCapabilities.areSizeAndRateSupported(maxWidth, maxHeight, fps)) {
        if (maxHeight >= 4320 && videoCapabilities.areSizeAndRateSupported(7680, 4320, fps)) {
          maxWidth = 7680;
          maxHeight = 4320;
        } else if (maxHeight >= 2160
            && videoCapabilities.areSizeAndRateSupported(3840, 2160, fps)) {
          maxWidth = 3840;
          maxHeight = 2160;
        } else if (maxHeight >= 1080
            && videoCapabilities.areSizeAndRateSupported(1920, 1080, fps)) {
          maxWidth = 1920;
          maxHeight = 1080;
        } else {
          Log.e(TAG, "Failed to find a compatible resolution");
          maxWidth = 1920;
          maxHeight = 1080;
        }
      }
    } else {
      if (maxHeight >= 2160 && videoCapabilities.isSizeSupported(3840, 2160)) {
        maxWidth = 3840;
        maxHeight = 2160;
      } else if (maxHeight >= 1080 && videoCapabilities.isSizeSupported(1920, 1080)) {
        maxWidth = 1920;
        maxHeight = 1080;
      } else {
        Log.e(TAG, "Failed to find a compatible resolution");
        maxWidth = 1920;
        maxHeight = 1080;
      }
    }

    if (!bridge.configureVideo(
        mediaFormat,
        surface,
        crypto,
        0,
        true,
        maxWidth,
        maxHeight,
        outCreateMediaCodecBridgeResult)) {
      Log.e(TAG, "Failed to configure video codec.");
      bridge.release();
      // outCreateMediaCodecBridgeResult.mErrorMessage is set inside configureVideo() on error.
      return;
    }
    if (!bridge.start()) {
      Log.e(TAG, "Failed to start video codec.");
      bridge.release();
      outCreateMediaCodecBridgeResult.mErrorMessage = "Failed to start video codec";
      return;
    }

    outCreateMediaCodecBridgeResult.mMediaCodecBridge = bridge;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void release() {
    try {
      String codecName = mMediaCodec.getName();
      Log.w(TAG, "calling MediaCodec.release() on " + codecName);
      mMediaCodec.release();
    } catch (IllegalStateException e) {
      // The MediaCodec is stuck in a wrong state, possibly due to losing
      // the surface.
      Log.e(TAG, "Cannot release media codec", e);
    }
    mMediaCodec = null;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean start() {
    try {
      mMediaCodec.start();
    } catch (IllegalStateException | IllegalArgumentException e) {
      Log.e(TAG, "Cannot start the media codec", e);
      return false;
    }
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void setPlaybackRate(double playbackRate) {
    if (mPlaybackRate == playbackRate) {
      return;
    }
    mPlaybackRate = playbackRate;
    if (mFrameRateEstimator != null) {
      updateOperatingRate();
    }
  }

  private void updateOperatingRate() {
    // We needn't set operation rate if playback rate is 0 or less.
    if (Double.compare(mPlaybackRate, 0.0) <= 0) {
      return;
    }
    if (mFps == FrameRateEstimator.INVALID_FRAME_RATE) {
      return;
    }
    if (mFps <= 0) {
      Log.e(TAG, "Failed to set operating rate with invalid fps " + mFps);
      return;
    }
    double operatingRate = mPlaybackRate * mFps;
    Bundle b = new Bundle();
    b.putFloat(MediaFormat.KEY_OPERATING_RATE, (float) operatingRate);
    try {
      mMediaCodec.setParameters(b);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to set MediaCodec operating rate", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int flush() {
    try {
      mFlushed = true;
      mMediaCodec.flush();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to flush MediaCodec", e);
      return MEDIA_CODEC_ERROR;
    }
    return MEDIA_CODEC_OK;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void stop() {
    synchronized (this) {
      mNativeMediaCodecBridge = 0;
    }
    try {
      mMediaCodec.stop();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to stop MediaCodec", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private String getName() {
    String codecName = "unknown";
    try {
      codecName = mMediaCodec.getName();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Cannot get codec name", e);
    }
    return codecName;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void getOutputFormat(GetOutputFormatResult outGetOutputFormatResult) {
    MediaFormat format = null;
    int status = MEDIA_CODEC_OK;
    try {
      format = mMediaCodec.getOutputFormat();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to get output format", e);
      status = MEDIA_CODEC_ERROR;
    }
    outGetOutputFormatResult.mStatus = status;
    outGetOutputFormatResult.mFormat = format;
  }

  /** Returns null if MediaCodec throws IllegalStateException. */
  @SuppressWarnings("unused")
  @UsedByNative
  private ByteBuffer getInputBuffer(int index) {
    try {
      return mMediaCodec.getInputBuffer(index);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to get input buffer", e);
      return null;
    }
  }

  /** Returns null if MediaCodec throws IllegalStateException. */
  @SuppressWarnings("unused")
  @UsedByNative
  private ByteBuffer getOutputBuffer(int index) {
    try {
      return mMediaCodec.getOutputBuffer(index);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to get output buffer", e);
      return null;
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int queueInputBuffer(
      int index, int offset, int size, long presentationTimeUs, int flags) {
    resetLastPresentationTimeIfNeeded(presentationTimeUs);
    try {
      mMediaCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
    } catch (Exception e) {
      Log.e(TAG, "Failed to queue input buffer", e);
      return MEDIA_CODEC_ERROR;
    }
    return MEDIA_CODEC_OK;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void setVideoBitrate(int bps, int frameRate) {
    int targetBps = bps;
    if (mBitrateAdjustmentType == BitrateAdjustmentTypes.FRAMERATE_ADJUSTMENT && frameRate > 0) {
      targetBps = BITRATE_ADJUSTMENT_FPS * bps / frameRate;
    }

    Bundle b = new Bundle();
    b.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, targetBps);
    try {
      mMediaCodec.setParameters(b);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to set MediaCodec parameters", e);
    }
    Log.v(TAG, "setVideoBitrate: input " + bps + "bps@" + frameRate + ", targetBps " + targetBps);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void requestKeyFrameSoon() {
    Bundle b = new Bundle();
    b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
    try {
      mMediaCodec.setParameters(b);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to set MediaCodec parameters", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int queueSecureInputBuffer(
      int index,
      int offset,
      byte[] iv,
      byte[] keyId,
      int[] numBytesOfClearData,
      int[] numBytesOfEncryptedData,
      int numSubSamples,
      int cipherMode,
      int patternEncrypt,
      int patternSkip,
      long presentationTimeUs) {
    resetLastPresentationTimeIfNeeded(presentationTimeUs);
    try {
      boolean usesCbcs =
          Build.VERSION.SDK_INT >= 24 && cipherMode == MediaCodec.CRYPTO_MODE_AES_CBC;

      if (usesCbcs) {
        Log.e(TAG, "Encryption scheme 'cbcs' not supported on this platform.");
        return MEDIA_CODEC_ERROR;
      }
      CryptoInfo cryptoInfo = new CryptoInfo();
      cryptoInfo.set(
          numSubSamples, numBytesOfClearData, numBytesOfEncryptedData, keyId, iv, cipherMode);
      if (patternEncrypt != 0 && patternSkip != 0) {
        if (usesCbcs) {
          // Above platform check ensured that setting the pattern is indeed supported.
          // MediaCodecUtil.setPatternIfSupported(cryptoInfo, patternEncrypt, patternSkip);
          Log.e(TAG, "Only AES_CTR is supported.");
        } else {
          Log.e(TAG, "Pattern encryption only supported for 'cbcs' scheme (CBC mode).");
          return MEDIA_CODEC_ERROR;
        }
      }
      mMediaCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
    } catch (MediaCodec.CryptoException e) {
      int errorCode = e.getErrorCode();
      if (errorCode == MediaCodec.CryptoException.ERROR_NO_KEY) {
        Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
        return MEDIA_CODEC_NO_KEY;
      } else if (errorCode == MediaCodec.CryptoException.ERROR_INSUFFICIENT_OUTPUT_PROTECTION) {
        Log.d(
            TAG,
            "Failed to queue secure input buffer: "
                + "CryptoException.ERROR_INSUFFICIENT_OUTPUT_PROTECTION");
        // Note that in Android OS version before 23, the MediaDrm class doesn't expose the current
        // key ids it holds.  In such case the Starboard media stack is unable to notify Cobalt of
        // the error via key statuses so MEDIA_CODEC_ERROR is returned instead to signal a general
        // media codec error.
        return Build.VERSION.SDK_INT >= 23
            ? MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION
            : MEDIA_CODEC_ERROR;
      }
      Log.e(
          TAG,
          "Failed to queue secure input buffer, CryptoException with error code "
              + e.getErrorCode());
      return MEDIA_CODEC_ERROR;
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to queue secure input buffer, IllegalStateException " + e);
      return MEDIA_CODEC_ERROR;
    }
    return MEDIA_CODEC_OK;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void releaseOutputBuffer(int index, boolean render) {
    try {
      mMediaCodec.releaseOutputBuffer(index, render);
    } catch (IllegalStateException e) {
      // TODO: May need to report the error to the caller. crbug.com/356498.
      Log.e(TAG, "Failed to release output buffer", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void releaseOutputBuffer(int index, long renderTimestampNs) {
    try {
      mMediaCodec.releaseOutputBuffer(index, renderTimestampNs);
    } catch (IllegalStateException e) {
      // TODO: May need to report the error to the caller. crbug.com/356498.
      Log.e(TAG, "Failed to release output buffer", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean configureVideo(
      MediaFormat format,
      Surface surface,
      MediaCrypto crypto,
      int flags,
      boolean allowAdaptivePlayback,
      int maxSupportedWidth,
      int maxSupportedHeight,
      CreateMediaCodecBridgeResult outCreateMediaCodecBridgeResult) {
    try {
      // If adaptive playback is turned off by request, then treat it as
      // not supported.  Note that configureVideo is only called once
      // during creation, else this would prevent re-enabling adaptive
      // playback later.
      if (!allowAdaptivePlayback) {
        mAdaptivePlaybackSupported = false;
      }

      if (mAdaptivePlaybackSupported) {
        // Since we haven't passed the properties of the stream we're playing
        // down to this level, from our perspective, we could potentially
        // adapt up to 8k at any point. We thus request 8k buffers up front,
        // unless the decoder claims to not be able to do 8k, in which case
        // we're ok, since we would've rejected a 8k stream when canPlayType
        // was called, and then use those decoder values instead. We only
        // support 8k for API level 29 and above.
        if (Build.VERSION.SDK_INT > 28) {
          format.setInteger(MediaFormat.KEY_MAX_WIDTH, Math.min(7680, maxSupportedWidth));
          format.setInteger(MediaFormat.KEY_MAX_HEIGHT, Math.min(4320, maxSupportedHeight));
        } else {
          // Android 5.0/5.1 seems not support 8K. Fallback to 4K until we get a
          // better way to get maximum supported resolution.
          format.setInteger(MediaFormat.KEY_MAX_WIDTH, Math.min(3840, maxSupportedWidth));
          format.setInteger(MediaFormat.KEY_MAX_HEIGHT, Math.min(2160, maxSupportedHeight));
        }
      }
      maybeSetMaxInputSize(format);
      mMediaCodec.configure(format, surface, crypto, flags);
      mFrameRateEstimator = new FrameRateEstimator();
      return true;
    } catch (IllegalArgumentException e) {
      Log.e(TAG, "Cannot configure the video codec with IllegalArgumentException: ", e);
      outCreateMediaCodecBridgeResult.mErrorMessage =
          "Cannot configure the video codec with IllegalArgumentException: " + e.toString();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Cannot configure the video codec with IllegalStateException: ", e);
      outCreateMediaCodecBridgeResult.mErrorMessage =
          "Cannot configure the video codec with IllegalStateException: " + e.toString();
    } catch (MediaCodec.CryptoException e) {
      Log.e(TAG, "Cannot configure the video codec with CryptoException: ", e);
      outCreateMediaCodecBridgeResult.mErrorMessage =
          "Cannot configure the video codec with CryptoException: " + e.toString();
    } catch (Exception e) {
      Log.e(TAG, "Cannot configure the video codec with Exception: ", e);
      outCreateMediaCodecBridgeResult.mErrorMessage =
          "Cannot configure the video codec with Exception: " + e.toString();
    }
    return false;
  }

  public static MediaFormat createAudioFormat(String mime, int sampleRate, int channelCount) {
    return MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
  }

  private static MediaFormat createVideoDecoderFormat(
      String mime, int width, int height, VideoCapabilities videoCapabilities) {
    return MediaFormat.createVideoFormat(
        mime,
        alignDimension(width, videoCapabilities.getWidthAlignment()),
        alignDimension(height, videoCapabilities.getHeightAlignment()));
  }

  private static int alignDimension(int size, int alignment) {
    int ceilDivide = (size + alignment - 1) / alignment;
    return ceilDivide * alignment;
  }

  // Use some heuristics to set KEY_MAX_INPUT_SIZE (the size of the input buffers).
  // Taken from exoplayer:
  // https://github.com/google/ExoPlayer/blob/8595c65678a181296cdf673eacb93d8135479340/library/src/main/java/com/google/android/exoplayer/MediaCodecVideoTrackRenderer.java
  private void maybeSetMaxInputSize(MediaFormat format) {
    if (format.containsKey(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)) {
      // Already set. The source of the format may know better, so do nothing.
      return;
    }
    int maxHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
    if (mAdaptivePlaybackSupported && format.containsKey(MediaFormat.KEY_MAX_HEIGHT)) {
      maxHeight = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_HEIGHT));
    }
    int maxWidth = format.getInteger(MediaFormat.KEY_WIDTH);
    if (mAdaptivePlaybackSupported && format.containsKey(MediaFormat.KEY_MAX_WIDTH)) {
      maxWidth = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_WIDTH));
    }
    int maxPixels;
    int minCompressionRatio;
    switch (format.getString(MediaFormat.KEY_MIME)) {
      case MimeTypes.VIDEO_H264:
        if ("BRAVIA 4K 2015".equals(Build.MODEL)) {
          // The Sony BRAVIA 4k TV has input buffers that are too small for the calculated
          // 4k video maximum input size, so use the default value.
          return;
        }
        // Round up width/height to an integer number of macroblocks.
        maxPixels = ((maxWidth + 15) / 16) * ((maxHeight + 15) / 16) * 16 * 16;
        minCompressionRatio = 2;
        break;
      case MimeTypes.VIDEO_VP8:
        // VPX does not specify a ratio so use the values from the platform's SoftVPX.cpp.
        maxPixels = maxWidth * maxHeight;
        minCompressionRatio = 2;
        break;
      case MimeTypes.VIDEO_H265:
      case MimeTypes.VIDEO_VP9:
      case MimeTypes.VIDEO_AV1:
        maxPixels = maxWidth * maxHeight;
        minCompressionRatio = 4;
        break;
      default:
        // Leave the default max input size.
        return;
    }
    // Estimate the maximum input size assuming three channel 4:2:0 subsampled input frames.
    int maxInputSize = (maxPixels * 3) / (2 * minCompressionRatio);
    format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, maxInputSize);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean isAdaptivePlaybackSupported(int width, int height) {
    // If media codec has adaptive playback supported, then the max sizes
    // used during creation are only hints.
    return mAdaptivePlaybackSupported;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private static void setCodecSpecificData(MediaFormat format, int index, byte[] bytes) {
    // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
    // csd-1, and so on. See: http://developer.android.com/reference/android/media/MediaCodec.html
    // for details.
    String name;
    switch (index) {
      case 0:
        name = "csd-0";
        break;
      case 1:
        name = "csd-1";
        break;
      case 2:
        name = "csd-2";
        break;
      default:
        name = null;
        break;
    }
    if (name != null) {
      format.setByteBuffer(name, ByteBuffer.wrap(bytes));
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private static void setFrameHasADTSHeader(MediaFormat format) {
    format.setInteger(MediaFormat.KEY_IS_ADTS, 1);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean configureAudio(MediaFormat format, MediaCrypto crypto, int flags) {
    try {
      mMediaCodec.configure(format, null, crypto, flags);
      return true;
    } catch (IllegalArgumentException | IllegalStateException e) {
      Log.e(TAG, "Cannot configure the audio codec", e);
    } catch (MediaCodec.CryptoException e) {
      Log.e(TAG, "Cannot configure the audio codec: DRM error", e);
    } catch (Exception e) {
      Log.e(TAG, "Cannot configure the audio codec", e);
    }
    return false;
  }

  private void resetLastPresentationTimeIfNeeded(long presentationTimeUs) {
    if (mFlushed) {
      mLastPresentationTimeUs =
          Math.max(presentationTimeUs - MAX_PRESENTATION_TIMESTAMP_SHIFT_US, 0);
      mFlushed = false;
    }
  }

  @SuppressWarnings("deprecation")
  private int getAudioFormat(int channelCount) {
    switch (channelCount) {
      case 1:
        return AudioFormat.CHANNEL_OUT_MONO;
      case 2:
        return AudioFormat.CHANNEL_OUT_STEREO;
      case 4:
        return AudioFormat.CHANNEL_OUT_QUAD;
      case 6:
        return AudioFormat.CHANNEL_OUT_5POINT1;
      case 8:
        if (Build.VERSION.SDK_INT >= 23) {
          return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
        } else {
          return AudioFormat.CHANNEL_OUT_7POINT1;
        }
      default:
        return AudioFormat.CHANNEL_OUT_DEFAULT;
    }
  }

  private native void nativeOnMediaCodecError(
      long nativeMediaCodecBridge,
      boolean isRecoverable,
      boolean isTransient,
      String diagnosticInfo);

  private native void nativeOnMediaCodecInputBufferAvailable(
      long nativeMediaCodecBridge, int bufferIndex);

  private native void nativeOnMediaCodecOutputBufferAvailable(
      long nativeMediaCodecBridge,
      int bufferIndex,
      int flags,
      int offset,
      long presentationTimeUs,
      int size);

  private native void nativeOnMediaCodecOutputFormatChanged(long nativeMediaCodecBridge);

  private native void nativeOnMediaCodecFrameRendered(
      long nativeMediaCodecBridge, long presentationTimeUs, long renderAtSystemTimeNs);
}
