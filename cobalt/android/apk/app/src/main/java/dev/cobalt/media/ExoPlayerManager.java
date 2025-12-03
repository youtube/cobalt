// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

import android.content.Context;
import android.view.Surface;
import androidx.media3.common.ColorInfo;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.exoplayer.DefaultRenderersFactory;
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
    private Context context;
    private DefaultRenderersFactory renderersFactory;

    /** Filters software video codecs from ExoPlayer codec selection. */
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
                Log.i(dev.cobalt.media.Log.TAG,
                        String.format("MediaCodecUtil.getDecoderInfos() error %s", e));
                return defaultDecoderInfos;
            }
            // Skip video decoder filtering for emulators.
            if (IsEmulator.isEmulator()) {
                Log.i(dev.cobalt.media.Log.TAG, "Allowing all available decoders for emulator.");
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
                    Log.w(dev.cobalt.media.Log.TAG,
                            "Could not find a hardware accelerated video decoder, falling back to sofware decoders.");
                    return defaultDecoderInfos;
                }
                return hardwareVideoDecoderInfos;
            }
            return defaultDecoderInfos;
        }
    }

    public ExoPlayerManager(Context context) {
        this.context = context;
        renderersFactory = new DefaultRenderersFactory(context).setMediaCodecSelector(
                new FilteringMediaCodecSelector());
    }

    @CalledByNative
    public synchronized ExoPlayerBridge createExoPlayerBridge(long nativeExoPlayerBridge,
            ExoPlayerMediaSource audioSource, ExoPlayerMediaSource videoSource, Surface surface,
            boolean enableTunnelMode) {
        if (videoSource != null && (surface == null || !surface.isValid())) {
            Log.e(dev.cobalt.media.Log.TAG, "Cannot initialize ExoPlayer with an invalid surface.");
            return null;
        }

        return new ExoPlayerBridge(nativeExoPlayerBridge, context, renderersFactory, audioSource,
                videoSource, surface, enableTunnelMode);
    }

    @CalledByNative
    public static ExoPlayerMediaSource createAudioMediaSource(
            String mime, byte[] audioConfigurationData, int sampleRate, int channelCount) {
        Format.Builder builder =
                new Format.Builder().setSampleRate(sampleRate).setChannelCount(channelCount);

        if (mime.equals(MimeTypes.AUDIO_AAC)) {
            if (audioConfigurationData != null) {
                builder.setInitializationData(Collections.singletonList(audioConfigurationData));
            }
        } else if (mime.equals(MimeTypes.AUDIO_OPUS)) {
            if (audioConfigurationData == null) {
                Log.e(dev.cobalt.media.Log.TAG, "Opus stream is missing configuration data.");
                return null;
            }

            byte[][] csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                    sampleRate, audioConfigurationData);
            if (csds == null) {
                Log.e(dev.cobalt.media.Log.TAG, "Error parsing Opus config info");
                return null;
            }
            builder.setInitializationData(Arrays.asList(csds));
        }

        builder.setSampleMimeType(mime);

        return new ExoPlayerMediaSource(builder.build());
    }

    @CalledByNative
    public static ExoPlayerMediaSource createVideoMediaSource(
            String mime, int width, int height, int fps, int bitrate, ColorInfo colorInfo) {
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

        return new ExoPlayerMediaSource(builder.build());
    }

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
