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
 * Entry point for creating ExoPlayer components from the native layer.
 *
 * <p>This manager provides factory methods for creating the {@link ExoPlayerBridge},
 * DRM components, and various {@link MediaSource} instances tailored for Starboard's
 * playback requirements.
 */
@JNINamespace("starboard")
public class ExoPlayerManager {
    private final Context mContext;
    private final DefaultRenderersFactory mRenderersFactory;
    private static final int PASSTHROUGH_CODEC_BITRATE = 384000;

    /**
     * A {@link MediaCodecSelector} that enforces Cobalt's decoding policies.
     *
     * <p>Policies:
     * <ul>
     *   <li>Enforces hardware-only video decoding on non-emulator devices to ensure performance.
     *   <li>Disables internal software decoders for AC3, E-AC3, and E-AC3-JOC, as these
     *       must be handled via passthrough.
     * </ul>
     */
    private static final class FilteringMediaCodecSelector implements MediaCodecSelector {
        @Override
        public List<MediaCodecInfo> getDecoderInfos(
                String mimeType, boolean requiresSecureDecoder, boolean requiresTunnelingDecoder) {
            // Ignore on-device AC3/EAC3 decoders as we only support passthrough.
            if (MimeTypes.AUDIO_AC3.equals(mimeType)
                    || MimeTypes.AUDIO_E_AC3.equals(mimeType)
                    || MimeTypes.AUDIO_E_AC3_JOC.equals(mimeType)) {
                return Collections.emptyList();
            }
            List<MediaCodecInfo> defaultDecoderInfos = null;
            try {
                defaultDecoderInfos =
                        androidx.media3.exoplayer.mediacodec.MediaCodecUtil.getDecoderInfos(
                                mimeType, requiresSecureDecoder, requiresTunnelingDecoder);
            } catch (androidx.media3.exoplayer.mediacodec.MediaCodecUtil.DecoderQueryException e) {
                Log.i(TAG, String.format("MediaCodecUtil.getDecoderInfos() error %s", e));
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

    @OptIn(markerClass = UnstableApi.class)
    @CalledByNative
    public static ExoPlayerMediaSource createAudioMediaSource(String mime,
            byte[] audioConfigurationData, int sampleRate, int channelCount,
            DrmSessionManager drmSessionManager, byte[] drmInitData) {
        Format.Builder builder = new Format.Builder()
                                         .setSampleRate(sampleRate)
                                         .setChannelCount(channelCount)
                                         .setSampleMimeType(mime);

        if (audioConfigurationData != null) {
            if (mime.equals(MimeTypes.AUDIO_OPUS)) {
                byte[][] csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                        sampleRate, audioConfigurationData);
                if (csds == null) {
                    Log.e(TAG, "Error parsing Opus config info");
                    return null;
                }
                builder.setInitializationData(Arrays.asList(csds));
            } else {
                builder.setInitializationData(Collections.singletonList(audioConfigurationData));
            }
        }

        if (mime.equals(MimeTypes.AUDIO_AC3) || mime.equals(MimeTypes.AUDIO_E_AC3)) {
            builder.setAverageBitrate(PASSTHROUGH_CODEC_BITRATE);
        }

        if (drmSessionManager != null) {
            builder.setCryptoType(C.CRYPTO_TYPE_FRAMEWORK);
            String schemeMimeType = "audio/mp4";
            if (mime.equals(MimeTypes.AUDIO_OPUS)) {
                schemeMimeType = "audio/webm";
            }
            builder.setDrmInitData(
                    new DrmInitData(new SchemeData(C.WIDEVINE_UUID, schemeMimeType, drmInitData)));
        }
        return new ExoPlayerMediaSource(builder.build(), drmSessionManager);
    }

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
            builder.setCryptoType(C.CRYPTO_TYPE_FRAMEWORK);
            String schemeMimeType = "video/mp4";
            if (mime.equals(MimeTypes.VIDEO_VP9)) {
                schemeMimeType = "video/webm";
            }
            builder.setDrmInitData(
                    new DrmInitData(new SchemeData(C.WIDEVINE_UUID, schemeMimeType, drmInitData)));
        }

        return new ExoPlayerMediaSource(builder.build(), drmSessionManager);
    }

    /**
     * Creates a {@link ColorInfo} object including HDR static metadata.
     *
     * <p>The HDR metadata is parsed into the CTA-861.3 binary format required by {@link
     * android.media.MediaFormat}.
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
