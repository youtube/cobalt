// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;

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
  private static final int MAXIMUM_INITIAL_FPS = 30;

  private MediaCodec mMediaCodec;
  private boolean mFlushed;
  private long mLastPresentationTimeUs;
  private final String mMime;
  private boolean mAdaptivePlaybackSupported;

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
  }

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
        float minMasteringLuminance) {
      this.colorRange = colorRange;
      this.colorStandard = colorStandard;
      this.colorTransfer = colorTransfer;

      // This logic is inspired by
      // https://github.com/google/ExoPlayer/blob/deb9b301b2c7ef66fdd7d8a3e58298a79ba9c619/library/core/src/main/java/com/google/android/exoplayer2/extractor/mkv/MatroskaExtractor.java#L1803.
      byte[] hdrStaticInfoData = new byte[25];
      ByteBuffer hdrStaticInfo = ByteBuffer.wrap(hdrStaticInfoData);
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
      hdrStaticInfo.putShort((short) DEFAULT_MAX_CLL);
      hdrStaticInfo.putShort((short) DEFAULT_MAX_FALL);
      this.hdrStaticInfo = hdrStaticInfo;
    }
  }

  private MediaCodecBridge(
      MediaCodec mediaCodec,
      String mime,
      boolean adaptivePlaybackSupported,
      BitrateAdjustmentTypes bitrateAdjustmentType) {
    if (mediaCodec == null) {
      throw new IllegalArgumentException();
    }
    mMediaCodec = mediaCodec;
    mMime = mime; // TODO: Delete the unused mMime field
    mLastPresentationTimeUs = 0;
    mFlushed = true;
    mAdaptivePlaybackSupported = adaptivePlaybackSupported;
    mBitrateAdjustmentType = bitrateAdjustmentType;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public static MediaCodecBridge createAudioMediaCodecBridge(
      String mime,
      boolean isSecure,
      boolean requireSoftwareCodec,
      int sampleRate,
      int channelCount,
      MediaCrypto crypto) {
    MediaCodec mediaCodec = null;
    try {
      String decoderName = MediaCodecUtil.findAudioDecoder(mime, 0);
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
        new MediaCodecBridge(mediaCodec, mime, true, BitrateAdjustmentTypes.NO_ADJUSTMENT);

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
  public static MediaCodecBridge createVideoMediaCodecBridge(
      String mime,
      boolean isSecure,
      boolean requireSoftwareCodec,
      int width,
      int height,
      Surface surface,
      MediaCrypto crypto,
      ColorInfo colorInfo) {
    MediaCodec mediaCodec = null;
    MediaCodecUtil.FindVideoDecoderResult findVideoDecoderResult =
        MediaCodecUtil.findVideoDecoder(mime, isSecure, 0, 0, 0, 0);
    try {
      String decoderName = findVideoDecoderResult.name;
      if (decoderName.equals("") || findVideoDecoderResult.videoCapabilities == null) {
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
        new MediaCodecBridge(mediaCodec, mime, true, BitrateAdjustmentTypes.NO_ADJUSTMENT);
    MediaFormat mediaFormat =
        createVideoDecoderFormat(mime, width, height, findVideoDecoderResult.videoCapabilities);

    boolean shouldConfigureHdr =
        android.os.Build.VERSION.SDK_INT >= 24
            && colorInfo != null
            && MediaCodecUtil.isHdrCapableVp9Decoder(findVideoDecoderResult);
    if (shouldConfigureHdr) {
      Log.d(TAG, "Setting HDR info.");
      mediaFormat.setInteger(MediaFormat.KEY_COLOR_TRANSFER, colorInfo.colorTransfer);
      mediaFormat.setInteger(MediaFormat.KEY_COLOR_STANDARD, colorInfo.colorStandard);
      mediaFormat.setInteger(MediaFormat.KEY_COLOR_RANGE, colorInfo.colorRange);
      mediaFormat.setByteBuffer(MediaFormat.KEY_HDR_STATIC_INFO, colorInfo.hdrStaticInfo);
    }

    int maxWidth = findVideoDecoderResult.videoCapabilities.getSupportedWidths().getUpper();
    int maxHeight = findVideoDecoderResult.videoCapabilities.getSupportedHeights().getUpper();
    if (!bridge.configureVideo(mediaFormat, surface, crypto, 0, true, maxWidth, maxHeight)) {
      Log.e(TAG, "Failed to configure video codec.");
      bridge.release();
      return null;
    }
    if (!bridge.start()) {
      Log.e(TAG, "Failed to start video codec.");
      bridge.release();
      return null;
    }

    return bridge;
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
  private void dequeueInputBuffer(long timeoutUs, DequeueInputResult outDequeueInputResult) {
    int status = MEDIA_CODEC_ERROR;
    int index = -1;
    try {
      int indexOrStatus = mMediaCodec.dequeueInputBuffer(timeoutUs);
      if (indexOrStatus >= 0) { // index!
        status = MEDIA_CODEC_OK;
        index = indexOrStatus;
      } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
        status = MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER;
      } else {
        throw new AssertionError("Unexpected index_or_status: " + indexOrStatus);
      }
    } catch (Exception e) {
      Log.e(TAG, "Failed to dequeue input buffer", e);
    }
    outDequeueInputResult.mStatus = status;
    outDequeueInputResult.mIndex = index;
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

  @SuppressWarnings({"unused", "deprecation"})
  @UsedByNative
  private void dequeueOutputBuffer(long timeoutUs, DequeueOutputResult outDequeueOutputResult) {
    int status = MEDIA_CODEC_ERROR;
    int index = -1;
    try {
      int indexOrStatus = mMediaCodec.dequeueOutputBuffer(info, timeoutUs);
      if (info.presentationTimeUs < mLastPresentationTimeUs) {
        // TODO: return a special code through DequeueOutputResult
        // to notify the native code that the frame has a wrong presentation
        // timestamp and should be skipped.
        info.presentationTimeUs = mLastPresentationTimeUs;
      }
      mLastPresentationTimeUs = info.presentationTimeUs;

      if (indexOrStatus >= 0) { // index!
        status = MEDIA_CODEC_OK;
        index = indexOrStatus;
      } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
        status = MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED;
      } else if (indexOrStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        status = MEDIA_CODEC_OUTPUT_FORMAT_CHANGED;
        MediaFormat newFormat = mMediaCodec.getOutputFormat();
      } else if (indexOrStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
        status = MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER;
      } else {
        throw new AssertionError("Unexpected index_or_status: " + indexOrStatus);
      }
    } catch (IllegalStateException e) {
      status = MEDIA_CODEC_ERROR;
      Log.e(TAG, "Failed to dequeue output buffer", e);
    }

    outDequeueOutputResult.mStatus = status;
    outDequeueOutputResult.mIndex = index;
    outDequeueOutputResult.mFlags = info.flags;
    outDequeueOutputResult.mOffset = info.offset;
    outDequeueOutputResult.mPresentationTimeMicroseconds = info.presentationTimeUs;
    outDequeueOutputResult.mNumBytes = info.size;
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
      int maxSupportedHeight) {
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
        // adapt up to 4k at any point.  We thus request 4k buffers up front,
        // unless the decoder claims to not be able to do 4k, in which case
        // we're ok, since we would've rejected a 4k stream when canPlayType
        // was called, and then use those decoder values instead.
        int maxWidth = Math.min(3840, maxSupportedWidth);
        int maxHeight = Math.min(2160, maxSupportedHeight);
        format.setInteger(MediaFormat.KEY_MAX_WIDTH, maxWidth);
        format.setInteger(MediaFormat.KEY_MAX_HEIGHT, maxHeight);
      }
      maybeSetMaxInputSize(format);
      mMediaCodec.configure(format, surface, crypto, flags);
      return true;
    } catch (IllegalArgumentException e) {
      Log.e(TAG, "Cannot configure the video codec, wrong format or surface", e);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Cannot configure the video codec", e);
    } catch (MediaCodec.CryptoException e) {
      Log.e(TAG, "Cannot configure the video codec: DRM error", e);
    } catch (Exception e) {
      Log.e(TAG, "Cannot configure the video codec", e);
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
}
