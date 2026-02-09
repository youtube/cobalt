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

import static androidx.media3.common.C.CRYPTO_TYPE_FRAMEWORK;
import static dev.cobalt.media.Log.TAG;

import android.content.Context;
import android.view.Surface;
import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.ColorInfo;
import androidx.media3.common.DrmInitData;
import androidx.media3.common.DrmInitData.SchemeData;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.drm.DrmSessionManager;
import androidx.media3.exoplayer.mediacodec.MediaCodecInfo;
import androidx.media3.exoplayer.mediacodec.MediaCodecSelector;
import dev.cobalt.util.IsEmulator;
import dev.cobalt.util.Log;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Passes the app Context to the ExoPlayerBridge upon creation, and implements utility functions
 * for initializing the player.
 */
@JNINamespace("starboard")
public class ExoPlayerManager {
    private final Context mContext;
    private final DefaultRenderersFactory mRenderersFactory;

    /** Filters software video codecs from ExoPlayer codec selection for non-emulator devices. */
    private static final class FilteringMediaCodecSelector implements MediaCodecSelector {
        @Override
        public List<MediaCodecInfo> getDecoderInfos(
                String mimeType, boolean requiresSecureDecoder, boolean requiresTunnelingDecoder) {
            List<MediaCodecInfo> defaultDecoderInfos = null;
            try {
                defaultDecoderInfos =
                        androidx.media3.exoplayer.mediacodec.MediaCodecUtil.getDecoderInfos(
                                mimeType, requiresSecureDecoder, requiresTunnelingDecoder);
            } catch (androidx.media3.exoplayer.mediacodec.MediaCodecUtil.DecoderQueryException e) {
                Log.i(TAG,
                        String.format("MediaCodecUtil.getDecoderInfos() error %s", e));
                return defaultDecoderInfos;
            }
            // Skip video decoder filtering for emulators.
            if (IsEmulator.isEmulator()) {
                Log.i(TAG, "Allowing all available decoders for emulator.");
                return defaultDecoderInfos;
            }

            List<MediaCodecInfo> hardwareVideoDecoderInfos = new ArrayList<>();
            if (mimeType.startsWith("video/")) {
                for (MediaCodecInfo decoderInfo : defaultDecoderInfos) {
                    if (decoderInfo.hardwareAccelerated) {
                        hardwareVideoDecoderInfos.add(decoderInfo);
                    }
                }

                if (hardwareVideoDecoderInfos.isEmpty()) {
                    Log.w(TAG,
                            "Could not find a hardware accelerated video decoder, falling back to software decoders.");
                    return defaultDecoderInfos;
                }
                return hardwareVideoDecoderInfos;
            }
            return defaultDecoderInfos;
        }
    }

    /**
     * Initializes a new ExoPlayerManager.
     * @param context The application context.
     */
    public ExoPlayerManager(Context context) {
        this.mContext = context;
        mRenderersFactory = new DefaultRenderersFactory(mContext).setMediaCodecSelector(
                new FilteringMediaCodecSelector());
    }

    /**
     * Creates and initializes a new ExoPlayerBridge.
     * @param nativeExoPlayerBridge The pointer to the native ExoPlayerBridge.
     * @param audioSource The audio source.
     * @param videoSource The video source.
     * @param surface The rendering surface.
     * @param enableTunnelMode Whether to enable tunnel mode.
     * @return A new ExoPlayerBridge instance.
     */
    @CalledByNative
    public synchronized ExoPlayerBridge createExoPlayerBridge(long nativeExoPlayerBridge,
            ExoPlayerMediaSource audioSource, ExoPlayerMediaSource videoSource,
            ExoPlayerDrmBridge drmBridge, Surface surface, boolean enableTunnelMode) {
        if (videoSource != null && (surface == null || !surface.isValid())) {
            Log.e(TAG, "Cannot initialize ExoPlayer with an invalid surface.");
            return null;
        }

        return new ExoPlayerBridge(nativeExoPlayerBridge, mContext, mRenderersFactory, audioSource,
                videoSource, drmBridge, surface, enableTunnelMode);
    }

    @CalledByNative
    public synchronized ExoPlayerDrmBridge createExoPlayerDrmBridge(long nativeDrmSystem) {
        return new ExoPlayerDrmBridge(mContext, nativeDrmSystem);
    }

    /**
     * Creates an audio MediaSource for playback.
     * @param mime The audio mime type.
     * @param audioConfigurationData The audio configuration data for Opus or AAC.
     * @param sampleRate The audio sample rate.
     * @param channelCount The number of audio channels.
     * @return A new ExoPlayerMediaSource instance.
     */
    @OptIn(markerClass = UnstableApi.class)
    @CalledByNative
    public static ExoPlayerMediaSource createAudioMediaSource(String mime,
            byte[] audioConfigurationData, int sampleRate, int channelCount,
            DrmSessionManager drmSessionManager, byte[] drmInitData) {
        Format.Builder builder =
                new Format.Builder().setSampleRate(sampleRate).setChannelCount(channelCount);

        if (mime.equals(MimeTypes.AUDIO_AAC)) {
            if (audioConfigurationData != null) {
                builder.setInitializationData(Collections.singletonList(audioConfigurationData));
            }
        } else if (mime.equals(MimeTypes.AUDIO_OPUS)) {
            if (audioConfigurationData == null) {
                Log.e(TAG, "Opus stream is missing configuration data.");
                return null;
            }

            byte[][] csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                    sampleRate, audioConfigurationData);
            if (csds == null) {
                Log.e(TAG, "Error parsing Opus config info");
                return null;
            }
            builder.setInitializationData(Arrays.asList(csds));
        } else {
          Log.i(TAG, String.format("Trying to create an audio format for mime %s", mime));
        }

        builder.setSampleMimeType(mime);
        if (drmSessionManager != null) {
          Log.i(TAG, "CREATED ENCRYPTED AUDIO MEDIA SOURCE");
          builder.setCryptoType(C.CRYPTO_TYPE_FRAMEWORK);
          String schemeMimeType = "audio/mp4";
          if (mime.equals(MimeTypes.AUDIO_OPUS)) {
            schemeMimeType = "audio/webm";
          }
          builder.setDrmInitData(new DrmInitData(new SchemeData(C.WIDEVINE_UUID, schemeMimeType, drmInitData)));
        }
        return new ExoPlayerMediaSource(builder.build(), drmSessionManager);
    }

    /**
     * Creates a video MediaSource for playback.
     * @param mime The video mime type.
     * @param width The video width.
     * @param height The video height.
     * @param fps The video frame rate.
     * @param bitrate The video bitrate.
     * @param colorInfo The video color info.
     * @return A new ExoPlayerMediaSource instance.
     */
    @CalledByNative
    public static ExoPlayerMediaSource createVideoMediaSource(String mime, int width, int height,
            int fps, int bitrate, ColorInfo colorInfo, DrmSessionManager drmSessionManager,
            byte[] drmInitData) {
        Format.Builder builder = new Format.Builder();
        builder.setSampleMimeType(mime).setWidth(width).setHeight(height);

        if (fps != -1) {
            builder.setFrameRate(fps);
        }

        if (bitrate != -1) {
            builder.setAverageBitrate(bitrate);
        }

        if (colorInfo != null) {
            builder.setColorInfo(colorInfo);
        }

      if (drmSessionManager != null) {
        Log.i(TAG, "CREATED ENCRYPTED VIDEO MEDIA SOURCE");
        builder.setCryptoType(C.CRYPTO_TYPE_FRAMEWORK);
        String schemeMimeType = "video/mp4";
        if (mime.equals(MimeTypes.VIDEO_VP9)) {
          schemeMimeType = "video/webm";
        }
        builder.setDrmInitData(new DrmInitData(new SchemeData(C.WIDEVINE_UUID, schemeMimeType, drmInitData)));
      }

        return new ExoPlayerMediaSource(builder.build(), drmSessionManager);
    }

    /**
     * Creates a ColorInfo object from the given HDR metadata.
     * @param colorRange The color range.
     * @param colorSpace The color space.
     * @param colorTransfer The color transfer.
     * @param primaryRChromaticityX The red primary chromaticity x-coordinate.
     * @param primaryRChromaticityY The red primary chromaticity y-coordinate.
     * @param primaryGChromaticityX The green primary chromaticity x-coordinate.
     * @param primaryGChromaticityY The green primary chromaticity y-coordinate.
     * @param primaryBChromaticityX The blue primary chromaticity x-coordinate.
     * @param primaryBChromaticityY The blue primary chromaticity y-coordinate.
     * @param whitePointChromaticityX The white point chromaticity x-coordinate.
     * @param whitePointChromaticityY The white point chromaticity y-coordinate.
     * @param maxMasteringLuminance The maximum mastering luminance.
     * @param minMasteringLuminance The minimum mastering luminance.
     * @param maxCll The maximum content light level.
     * @param maxFall The maximum frame-average light level.
     * @return A new ColorInfo instance.
     */
    @CalledByNative
    public static ColorInfo createExoPlayerColorInfo(int colorRange, int colorSpace,
            int colorTransfer, float primaryRChromaticityX, float primaryRChromaticityY,
            float primaryGChromaticityX, float primaryGChromaticityY, float primaryBChromaticityX,
            float primaryBChromaticityY, float whitePointChromaticityX,
            float whitePointChromaticityY, float maxMasteringLuminance, float minMasteringLuminance,
            int maxCll, int maxFall) {
        ByteBuffer hdrStaticInfo = dev.cobalt.media.MediaCodecUtil.getHdrStaticInfo(
                primaryRChromaticityX, primaryRChromaticityY, primaryGChromaticityX,
                primaryGChromaticityY, primaryBChromaticityX, primaryBChromaticityY,
                whitePointChromaticityX, whitePointChromaticityY, maxMasteringLuminance,
                minMasteringLuminance, maxCll, maxFall, false);
        return new ColorInfo.Builder()
                .setColorRange(colorRange)
                .setColorSpace(colorSpace)
                .setColorTransfer(colorTransfer)
                .setHdrStaticInfo(hdrStaticInfo.array())
                .build();
    }
}
