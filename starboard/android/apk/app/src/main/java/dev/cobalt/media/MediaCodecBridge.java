// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCodec.CryptoInfo.Pattern;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.view.Surface;
import dev.cobalt.util.Log;
import dev.cobalt.util.SynchronizedHolder;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;
import java.util.Optional;

/** A wrapper of the MediaCodec class. */
@SuppressWarnings("unused")
@UsedByNative
class MediaCodecBridge {
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
  private final SynchronizedHolder<MediaCodec, IllegalStateException> mMediaCodec =
      new SynchronizedHolder<>(() -> new IllegalStateException("MediaCodec was destroyed"));

  private MediaCodec.Callback mCallback;
  private boolean mFlushed;
  private long mLastPresentationTimeUs;
  private final String mMime;
  private double mPlaybackRate = 1.0;
  private int mFps = 30;

  private MediaCodec.OnFrameRenderedListener mFrameRendererListener;

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
        Log.v(TAG, "Invalid output presentation timestamp.");
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
      mStatus = MediaCodecStatus.ERROR;
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
      mStatus = MediaCodecStatus.ERROR;
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
    // May be null if mStatus is not MediaCodecStatus.OK.
    private MediaFormat mFormat;
    private Optional<Boolean> mFormatHasCropValues = Optional.empty();

    @SuppressWarnings("unused")
    @UsedByNative
    private GetOutputFormatResult() {
      mStatus = MediaCodecStatus.ERROR;
      mFormat = null;
    }

    private boolean formatHasCropValues() {
      if (!mFormatHasCropValues.isPresent() && mFormat != null) {
        boolean hasCropValues =
            mFormat.containsKey(KEY_CROP_RIGHT)
                && mFormat.containsKey(KEY_CROP_LEFT)
                && mFormat.containsKey(KEY_CROP_BOTTOM)
                && mFormat.containsKey(KEY_CROP_TOP);
        mFormatHasCropValues = Optional.ofNullable(hasCropValues);
      }
      return mFormatHasCropValues.orElse(false);
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int status() {
      return mStatus;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int textureWidth() {
      return (mFormat != null && mFormat.containsKey(MediaFormat.KEY_WIDTH))
          ? mFormat.getInteger(MediaFormat.KEY_WIDTH)
          : 0;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int textureHeight() {
      return (mFormat != null && mFormat.containsKey(MediaFormat.KEY_HEIGHT))
          ? mFormat.getInteger(MediaFormat.KEY_HEIGHT)
          : 0;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int cropLeft() {
      return formatHasCropValues() ? mFormat.getInteger(KEY_CROP_LEFT) : -1;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int cropTop() {
      return formatHasCropValues() ? mFormat.getInteger(KEY_CROP_TOP) : -1;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int cropRight() {
      return formatHasCropValues() ? mFormat.getInteger(KEY_CROP_RIGHT) : -1;
    }

    @SuppressWarnings("unused")
    @UsedByNative
    private int cropBottom() {
      return formatHasCropValues() ? mFormat.getInteger(KEY_CROP_BOTTOM) : -1;
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

  public MediaCodecBridge(
      long nativeMediaCodecBridge,
      MediaCodec mediaCodec,
      String mime,
      BitrateAdjustmentTypes bitrateAdjustmentType,
      int tunnelModeAudioSessionId) {
    if (mediaCodec == null) {
      throw new IllegalArgumentException();
    }
    mNativeMediaCodecBridge = nativeMediaCodecBridge;
    mMediaCodec.set(mediaCodec);
    mMime = mime; // TODO: Delete the unused mMime field
    mLastPresentationTimeUs = 0;
    mFlushed = true;
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
          public void onCryptoError(MediaCodec codec, MediaCodec.CryptoException e) {
            synchronized (this) {
              if (mNativeMediaCodecBridge == 0) {
                return;
              }
              nativeOnMediaCodecCryotoError(
                  mNativeMediaCodecBridge,
                  e.getErrorCode());
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
              if (mFrameRateEstimator != null) {
                mFrameRateEstimator.reset();
              }
            }
          }
        };
    mMediaCodec.get().setCallback(mCallback);

    if (isFrameRenderedCallbackEnabled() || tunnelModeAudioSessionId != -1) {
      mFrameRendererListener =
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
      mMediaCodec.get().setOnFrameRenderedListener(mFrameRendererListener, null);
    }
  }

  @UsedByNative
  public static boolean isFrameRenderedCallbackEnabled() {
    // Starting with Android 14, onFrameRendered should be called accurately for each rendered
    // frame.
    return Build.VERSION.SDK_INT >= 34;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public static void createVideoMediaCodecBridge(
      long nativeMediaCodecBridge,
      String mime,
      String decoderName,
      // `widthHint` and `heightHint` are used to create the Android video format, which don't have
      // to be directly related to the resolution of the video.
      int widthHint,
      int heightHint,
      int fps,
      int maxWidth,
      int maxHeight,
      Surface surface,
      MediaCrypto crypto,
      ColorInfo colorInfo,
      int tunnelModeAudioSessionId,
      int maxVideoInputSize,
      CreateMediaCodecBridgeResult outCreateMediaCodecBridgeResult) {
    MediaCodec mediaCodec = null;
    outCreateMediaCodecBridgeResult.mMediaCodecBridge = null;

    if (decoderName.equals("")) {
      String message = "Invalid decoder name.";
      Log.e(TAG, message);
      outCreateMediaCodecBridgeResult.mErrorMessage = message;
      return;
    }

    try {
      Log.i(TAG, "Creating \"%s\" decoder.", decoderName);
      mediaCodec = MediaCodec.createByCodecName(decoderName);
    } catch (Exception e) {
      String message =
          String.format(
              Locale.US,
              "Failed to create MediaCodec: %s, mustSupportSecure: %s," + " DecoderName: %s",
              mime,
              crypto != null,
              decoderName);
      message += ", exception: " + e.toString();
      Log.e(TAG, message);
      outCreateMediaCodecBridgeResult.mErrorMessage = message;
      return;
    }
    if (mediaCodec == null) {
      outCreateMediaCodecBridgeResult.mErrorMessage = "mediaCodec is null";
      return;
    }

    MediaCodecInfo codecInfo = mediaCodec.getCodecInfo();
    if (codecInfo == null) {
      outCreateMediaCodecBridgeResult.mErrorMessage = "codecInfo is null";
      return;
    }
    CodecCapabilities codecCapabilities = codecInfo.getCapabilitiesForType(mime);
    if (codecCapabilities == null) {
      outCreateMediaCodecBridgeResult.mErrorMessage = "codecCapabilities is null";
      return;
    }
    VideoCapabilities videoCapabilities = codecCapabilities.getVideoCapabilities();
    if (videoCapabilities == null) {
      outCreateMediaCodecBridgeResult.mErrorMessage = "videoCapabilities is null";
      return;
    }

    MediaCodecBridge bridge =
        new MediaCodecBridge(
            nativeMediaCodecBridge,
            mediaCodec,
            mime,
            BitrateAdjustmentTypes.NO_ADJUSTMENT,
            tunnelModeAudioSessionId);
    MediaFormat mediaFormat =
        createVideoDecoderFormat(mime, widthHint, heightHint, videoCapabilities);

    boolean shouldConfigureHdr =
        colorInfo != null && MediaCodecUtil.isHdrCapableVideoDecoder(mime, codecCapabilities);
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
      Log.d(TAG, "Enabled tunnel mode playback on audio session " + tunnelModeAudioSessionId);
    }

    if (maxWidth > 0 && maxHeight > 0) {
      Log.i(TAG, "Evaluate maxWidth and maxHeight (%d, %d) passed in", maxWidth, maxHeight);
    } else {
      maxWidth = videoCapabilities.getSupportedWidths().getUpper();
      maxHeight = videoCapabilities.getSupportedHeights().getUpper();
      Log.i(
          TAG,
          "maxWidth and maxHeight not passed in, using result of getSupportedWidths()/Heights()"
              + " (%d, %d)",
          maxWidth,
          maxHeight);
    }

    if (fps > 0) {
      if (videoCapabilities.areSizeAndRateSupported(maxWidth, maxHeight, fps)) {
        Log.i(
            TAG,
            "Set maxWidth and maxHeight to (%d, %d)@%d per `areSizeAndRateSupported()`",
            maxWidth,
            maxHeight,
            fps);
      } else {
        Log.w(
            TAG,
            "maxWidth and maxHeight (%d, %d)@%d not supported per `areSizeAndRateSupported()`,"
                + " continue searching",
            maxWidth,
            maxHeight,
            fps);
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
        Log.i(
            TAG,
            "Set maxWidth and maxHeight to (%d, %d)@%d per `areSizeAndRateSupported()`",
            maxWidth,
            maxHeight,
            fps);
      }
    } else {
      if (maxHeight >= 480 && videoCapabilities.isSizeSupported(maxWidth, maxHeight)) {
        // Technically we can do this check for all resolutions, but only check for resolution with
        // height more than 480p to minimize production impact.  To use a lower resolution is more
        // to reduce memory footprint, and optimize for lower resolution isn't as helpful anyway.
        Log.i(
            TAG,
            "Set maxWidth and maxHeight to (%d, %d) per `isSizeSupported()`",
            maxWidth,
            maxHeight);
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
        Log.i(
            TAG,
            "Set maxWidth and maxHeight to (%d, %d) per `isSizeSupported()`",
            maxWidth,
            maxHeight);
      }
    }

    if (maxVideoInputSize > 0) {
      mediaFormat.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, maxVideoInputSize);
      try {
        Log.i(
            TAG,
            "Set KEY_MAX_INPUT_SIZE to "
                + maxVideoInputSize
                + " (actual: "
                + mediaFormat.getInteger(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)
                + ").");
      } catch (Exception e) {
        Log.e(TAG, "MediaFormat.getInteger(KEY_MAX_INPUT_SIZE) failed with exception: ", e);
      }
    }
    if (!bridge.configureVideo(
        mediaFormat, surface, crypto, 0, maxWidth, maxHeight, outCreateMediaCodecBridgeResult)) {
      Log.e(TAG, "Failed to configure video codec.");
      bridge.release();
      // outCreateMediaCodecBridgeResult.mErrorMessage is set inside configureVideo() on error.
      return;
    }
    if (!bridge.start(outCreateMediaCodecBridgeResult)) {
      Log.e(TAG, "Failed to start video codec.");
      bridge.release();
      // outCreateMediaCodecBridgeResult.mErrorMessage is set inside start() on error.
      return;
    }

    outCreateMediaCodecBridgeResult.mMediaCodecBridge = bridge;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void release() {
    try {
      String codecName = mMediaCodec.get().getName();
      Log.w(TAG, "calling MediaCodec.release() on " + codecName);
      mMediaCodec.get().release();
    } catch (Exception e) {
      // The MediaCodec is stuck in a wrong state, possibly due to losing
      // the surface.
      Log.e(TAG, "Cannot release media codec", e);
    }
    mMediaCodec.set(null);
  }

  public boolean start() {
    return start(null);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean restart() {
    // Restart MediaCodec after flush().
    return start(null);
  }

  @SuppressWarnings("unused")
  public boolean start(CreateMediaCodecBridgeResult outCreateMediaCodecBridgeResult) {
    try {
      mMediaCodec.get().start();
    } catch (IllegalStateException | IllegalArgumentException e) {
      Log.e(TAG, "Failed to start the media codec", e);
      if (outCreateMediaCodecBridgeResult != null) {
        outCreateMediaCodecBridgeResult.mErrorMessage =
            "Failed to start media codec " + e.toString();
      }
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
      mMediaCodec.get().setParameters(b);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to set MediaCodec operating rate", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private int flush() {
    try {
      mFlushed = true;
      mMediaCodec.get().flush();
    } catch (Exception e) {
      Log.e(TAG, "Failed to flush MediaCodec", e);
      return MediaCodecStatus.ERROR;
    } finally {
      if (mFrameRateEstimator != null) {
        mFrameRateEstimator.reset();
      }
    }
    return MediaCodecStatus.OK;
  }

  // It is required to reset mNativeMediaCodecBridge when the native media_codec_bridge object is
  // destroyed.
  @SuppressWarnings("unused")
  @UsedByNative
  private void resetNativeMediaCodecBridge() {
    synchronized (this) {
      mNativeMediaCodecBridge = 0;
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void stop() {
    resetNativeMediaCodecBridge();
    try {
      mMediaCodec.get().stop();
    } catch (Exception e) {
      Log.e(TAG, "Failed to stop MediaCodec", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private String getName() {
    String codecName = "unknown";
    try {
      codecName = mMediaCodec.get().getName();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Cannot get codec name", e);
    }
    return codecName;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void getOutputFormat(GetOutputFormatResult outGetOutputFormatResult) {
    MediaFormat format = null;
    int status = MediaCodecStatus.OK;
    try {
      format = mMediaCodec.get().getOutputFormat();
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to get output format", e);
      status = MediaCodecStatus.ERROR;
    }
    outGetOutputFormatResult.mStatus = status;
    outGetOutputFormatResult.mFormat = format;
  }

  /** Returns null if MediaCodec throws IllegalStateException. */
  @SuppressWarnings("unused")
  @UsedByNative
  private ByteBuffer getInputBuffer(int index) {
    try {
      return mMediaCodec.get().getInputBuffer(index);
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
      return mMediaCodec.get().getOutputBuffer(index);
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
      mMediaCodec.get().queueInputBuffer(index, offset, size, presentationTimeUs, flags);
    } catch (Exception e) {
      Log.e(TAG, "Failed to queue input buffer", e);
      return MediaCodecStatus.ERROR;
    }
    return MediaCodecStatus.OK;
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
      mMediaCodec.get().setParameters(b);
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
      mMediaCodec.get().setParameters(b);
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
      int blocksToEncrypt,
      int blocksToSkip,
      long presentationTimeUs) {
    resetLastPresentationTimeIfNeeded(presentationTimeUs);
    try {
      CryptoInfo cryptoInfo = new CryptoInfo();
      cryptoInfo.set(
          numSubSamples, numBytesOfClearData, numBytesOfEncryptedData, keyId, iv, cipherMode);

      if (cipherMode == MediaCodec.CRYPTO_MODE_AES_CBC) {
        cryptoInfo.setPattern(new Pattern(blocksToEncrypt, blocksToSkip));
      } else if (blocksToEncrypt != 0 || blocksToSkip != 0) {
        Log.e(TAG, "Pattern encryption only supported for 'cbcs' scheme (CBC mode).");
        return MediaCodecStatus.ERROR;
      }

      mMediaCodec.get().queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, 0);
    } catch (MediaCodec.CryptoException e) {
      int errorCode = e.getErrorCode();
      if (errorCode == MediaCodec.CryptoException.ERROR_NO_KEY) {
        Log.d(TAG, "Failed to queue secure input buffer: CryptoException.ERROR_NO_KEY");
        return MediaCodecStatus.NO_KEY;
      } else if (errorCode == MediaCodec.CryptoException.ERROR_INSUFFICIENT_OUTPUT_PROTECTION) {
        Log.d(
            TAG,
            "Failed to queue secure input buffer: "
                + "CryptoException.ERROR_INSUFFICIENT_OUTPUT_PROTECTION");
        return MediaCodecStatus.INSUFFICIENT_OUTPUT_PROTECTION;
      }
      Log.e(
          TAG,
          "Failed to queue secure input buffer, CryptoException with error code "
              + e.getErrorCode());
      return MediaCodecStatus.ERROR;
    } catch (IllegalArgumentException e) {
      Log.e(TAG, "Failed to queue secure input buffer, IllegalArgumentException " + e);
      return MediaCodecStatus.ERROR;
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to queue secure input buffer, IllegalStateException " + e);
      return MediaCodecStatus.ERROR;
    }
    return MediaCodecStatus.OK;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void releaseOutputBuffer(int index, boolean render) {
    try {
      mMediaCodec.get().releaseOutputBuffer(index, render);
    } catch (IllegalStateException e) {
      // TODO: May need to report the error to the caller. crbug.com/356498.
      Log.e(TAG, "Failed to release output buffer", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private void releaseOutputBuffer(int index, long renderTimestampNs) {
    try {
      mMediaCodec.get().releaseOutputBuffer(index, renderTimestampNs);
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
      int maxSupportedWidth,
      int maxSupportedHeight,
      CreateMediaCodecBridgeResult outCreateMediaCodecBridgeResult) {
    try {
      // Since we haven't passed the properties of the stream we're playing down to this level, from
      // our perspective, we could potentially adapt up to 8k at any point. We thus request 8k
      // buffers up front, unless the decoder claims to not be able to do 8k, in which case we're
      // ok, since we would've rejected a 8k stream when canPlayType was called, and then use those
      // decoder values instead. We only support 8k for API level 29 and above.
      if (Build.VERSION.SDK_INT > 28) {
        format.setInteger(MediaFormat.KEY_MAX_WIDTH, Math.min(7680, maxSupportedWidth));
        format.setInteger(MediaFormat.KEY_MAX_HEIGHT, Math.min(4320, maxSupportedHeight));
      } else {
        // Android 5.0/5.1 seems not support 8K. Fallback to 4K until we get a
        // better way to get maximum supported resolution.
        format.setInteger(MediaFormat.KEY_MAX_WIDTH, Math.min(3840, maxSupportedWidth));
        format.setInteger(MediaFormat.KEY_MAX_HEIGHT, Math.min(2160, maxSupportedHeight));
      }

      maybeSetMaxVideoInputSize(format);
      if (Build.VERSION.SDK_INT > 33 && crypto != null) {
        flags |= MediaCodec.CONFIGURE_FLAG_USE_CRYPTO_ASYNC;
      }
      mMediaCodec.get().configure(format, surface, crypto, flags);
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

  private static MediaFormat createVideoDecoderFormat(
      String mime, int widthHint, int heightHint, VideoCapabilities videoCapabilities) {
    return MediaFormat.createVideoFormat(
        mime,
        alignDimension(widthHint, videoCapabilities.getWidthAlignment()),
        alignDimension(heightHint, videoCapabilities.getHeightAlignment()));
  }

  private static int alignDimension(int size, int alignment) {
    int ceilDivide = (size + alignment - 1) / alignment;
    return ceilDivide * alignment;
  }

  // Use some heuristics to set KEY_MAX_INPUT_SIZE (the size of the input buffers).
  // Taken from ExoPlayer:
  // https://github.com/google/ExoPlayer/blob/8595c65678a181296cdf673eacb93d8135479340/library/src/main/java/com/google/android/exoplayer/MediaCodecVideoTrackRenderer.java
  private void maybeSetMaxVideoInputSize(MediaFormat format) {
    if (format.containsKey(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)) {
      try {
        Log.i(
            TAG,
            "Use default value for KEY_MAX_INPUT_SIZE: "
                + format.getInteger(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)
                + '.');
      } catch (Exception e) {
        Log.e(TAG, "MediaFormat.getInteger(KEY_MAX_INPUT_SIZE) failed with exception: ", e);
      }
      // Already set. The source of the format may know better, so do nothing.
      return;
    }
    int maxHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
    if (format.containsKey(MediaFormat.KEY_MAX_HEIGHT)) {
      maxHeight = Math.max(maxHeight, format.getInteger(MediaFormat.KEY_MAX_HEIGHT));
    }
    int maxWidth = format.getInteger(MediaFormat.KEY_WIDTH);
    if (format.containsKey(MediaFormat.KEY_MAX_WIDTH)) {
      maxWidth = Math.max(maxWidth, format.getInteger(MediaFormat.KEY_MAX_WIDTH));
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
    int maxVideoInputSize = (maxPixels * 3) / (2 * minCompressionRatio);
    format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, maxVideoInputSize);
    try {
      Log.i(
          TAG,
          "Set KEY_MAX_INPUT_SIZE to "
              + maxVideoInputSize
              + " (actual: "
              + format.getInteger(android.media.MediaFormat.KEY_MAX_INPUT_SIZE)
              + ").");
    } catch (Exception e) {
      Log.e(TAG, "MediaFormat.getInteger(KEY_MAX_INPUT_SIZE) failed with exception: ", e);
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public boolean configureAudio(MediaFormat format, MediaCrypto crypto, int flags) {
    try {
      if (Build.VERSION.SDK_INT > 33 && crypto != null) {
        flags |= MediaCodec.CONFIGURE_FLAG_USE_CRYPTO_ASYNC;
      }
      mMediaCodec.get().configure(format, null, crypto, flags);
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
        return AudioFormat.CHANNEL_OUT_7POINT1_SURROUND;
      default:
        return AudioFormat.CHANNEL_OUT_DEFAULT;
    }
  }

  private native void nativeOnMediaCodecError(
      long nativeMediaCodecBridge,
      boolean isRecoverable,
      boolean isTransient,
      String diagnosticInfo);

  private native void nativeOnMediaCodecCryotoError(
      long nativeMediaCodecBridge,
      int error_code);

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
