// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.compat.ApiHelperForN;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Iterator;
import java.util.Locale;
import java.util.NoSuchElementException;

/**
 * A collection of MediaCodec utility functions.
 */
@JNINamespace("media")
@MainDex
class MediaCodecUtil {
    private static final String TAG = "MediaCodecUtil";

    /**
     * Information returned by createDecoder()
     */
    public static class CodecCreationInfo {
        public MediaCodec mediaCodec;
        public boolean supportsAdaptivePlayback;
        public @BitrateAdjuster.Type int bitrateAdjuster = BitrateAdjuster.Type.NO_ADJUSTMENT;
    }

    public static final class MimeTypes {
        public static final String VIDEO_MP4 = "video/mp4";
        public static final String VIDEO_WEBM = "video/webm";
        public static final String VIDEO_H264 = "video/avc";
        public static final String VIDEO_DOLBY_VISION = "video/dolby-vision";
        public static final String VIDEO_HEVC = "video/hevc";
        public static final String VIDEO_VP8 = "video/x-vnd.on2.vp8";
        public static final String VIDEO_VP9 = "video/x-vnd.on2.vp9";
        public static final String VIDEO_AV1 = "video/av01";
        public static final String AUDIO_OPUS = "audio/opus";
    }

    /**
     * Class to abstract platform version API differences for interacting with
     * the MediaCodecList.
     */
    private static class MediaCodecListHelper implements Iterable<MediaCodecInfo> {
        public MediaCodecListHelper() {
            try {
                mCodecList = new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos();
            } catch (Throwable e) {
                // Swallow the exception due to bad Android implementation and pretend
                // MediaCodecList is not supported.
            }
        }

        @Override
        public Iterator<MediaCodecInfo> iterator() {
            return new CodecInfoIterator();
        }

        @SuppressWarnings("deprecation")
        private int getCodecCount() {
            if (hasNewMediaCodecList()) return mCodecList.length;
            try {
                return MediaCodecList.getCodecCount();
            } catch (RuntimeException e) {
                // Swallow the exception due to bad Android implementation and pretend
                // MediaCodecList is not supported.
                return 0;
            }
        }

        @SuppressWarnings("deprecation")
        private MediaCodecInfo getCodecInfoAt(int index) {
            if (hasNewMediaCodecList()) return mCodecList[index];
            return MediaCodecList.getCodecInfoAt(index);
        }

        private boolean hasNewMediaCodecList() {
            return mCodecList != null;
        }

        private MediaCodecInfo[] mCodecList;

        private class CodecInfoIterator implements Iterator<MediaCodecInfo> {
            private int mPosition;

            @Override
            public boolean hasNext() {
                return mPosition < getCodecCount();
            }

            @Override
            public MediaCodecInfo next() {
                if (mPosition == getCodecCount()) {
                    throw new NoSuchElementException();
                }
                return getCodecInfoAt(mPosition++);
            }

            @Override
            public void remove() {
                throw new UnsupportedOperationException();
            }
        }
    }

    /**
     * Return true if and only if info is a software codec.
     */
    private static boolean isSoftwareCodec(MediaCodecInfo info) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) return !info.isHardwareAccelerated();

        String name = info.getName().toLowerCase(Locale.ROOT);
        // This is taken from libstagefright/OMXCodec.cpp for pre codec2.
        if (name.startsWith("omx.google.")) return true;

        // Codec2 names sw decoders this way.
        // See hardware/google/av/codec2/vndk/C2Store.cpp.
        if (name.startsWith("c2.google.") || name.startsWith("c2.android.")) return true;

        return false;
    }

    /**
     * Get a name of default android codec.
     * @param mime MIME type of the media.
     * @param direction Whether this is encoder or decoder.
     * @param requireSoftwareCodec Whether we require a software codec.
     * @return name of the codec.
     */
    @CalledByNative
    private static String getDefaultCodecName(
            String mime, int direction, boolean requireSoftwareCodec) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        for (MediaCodecInfo info : codecListHelper) {
            int codecDirection =
                    info.isEncoder() ? MediaCodecDirection.ENCODER : MediaCodecDirection.DECODER;
            if (codecDirection != direction) continue;

            if (requireSoftwareCodec && !isSoftwareCodec(info)) continue;

            for (String supportedType : info.getSupportedTypes()) {
                if (supportedType.equalsIgnoreCase(mime)) return info.getName();
            }
        }

        Log.e(TAG, "Decoder for type %s is not supported on this device", mime);
        return "";
    }

    /**
     * Get a list of encoder supported color formats for specified MIME type.
     * @param mime MIME type of the media format.
     * @return a list of encoder supported color formats.
     */
    @CalledByNative
    private static int[] getEncoderColorFormatsForMime(String mime) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        for (MediaCodecInfo info : codecListHelper) {
            if (!info.isEncoder()) continue;

            for (String supportedType : info.getSupportedTypes()) {
                if (supportedType.equalsIgnoreCase(mime)) {
                    try {
                        return info.getCapabilitiesForType(supportedType).colorFormats;
                    } catch (IllegalArgumentException e) {
                        // Type is not supported.
                    }
                }
            }
        }
        return null;
    }

    /**
      * Check if a given MIME type can be decoded.
      * @param mime MIME type of the media.
      * @param secure Whether secure decoder is required.
      * @return true if system is able to decode, or false otherwise.
      */
    @CalledByNative
    private static boolean canDecode(String mime, boolean isSecure) {
        // Not supported on some devices.
        if (!isDecoderSupportedForDevice(mime)) {
            Log.e(TAG, "Decoder for type %s is not supported on this device", mime);
            return false;
        }

        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        if (codecListHelper.hasNewMediaCodecList()) {
            for (MediaCodecInfo info : codecListHelper) {
                if (info.isEncoder()) continue;

                try {
                    CodecCapabilities caps = info.getCapabilitiesForType(mime);
                    if (caps != null) {
                        // There may be multiple entries in the list for the same family
                        // (e.g. OMX.qcom.video.decoder.avc and OMX.qcom.video.decoder.avc.secure),
                        // so return early if this one matches what we're looking for.

                        // If a secure decoder is required, then FEATURE_SecurePlayback must be
                        // supported.
                        if (isSecure
                                && caps.isFeatureSupported(
                                        CodecCapabilities.FEATURE_SecurePlayback)) {
                            return true;
                        }

                        // If a secure decoder is not required, then make sure that
                        // FEATURE_SecurePlayback is not required. It may work for unsecure
                        // content, but keep scanning for another codec that supports
                        // unsecure content directly.
                        if (!isSecure
                                && !caps.isFeatureRequired(
                                        CodecCapabilities.FEATURE_SecurePlayback)) {
                            return true;
                        }
                    }
                } catch (IllegalArgumentException e) {
                    // Type is not supported.
                }
            }

            // Unable to find a match for |mime|, so not supported.
            return false;
        }

        // On older versions of Android attempt to create a decoder for the specified MIME type.
        // TODO(liberato): Should we insist on software here?
        CodecCreationInfo info = createDecoder(mime, isSecure ? CodecType.SECURE : CodecType.ANY);
        if (info == null || info.mediaCodec == null) return false;

        try {
            info.mediaCodec.release();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot release media codec", e);
        }
        return true;
    }

    /**
      * Needed on M and older to get correct information about VP9 support.
      * @param profileLevels The CodecProfileLevelList to add supported profile levels to.
      * @param videoCapabilities The MediaCodecInfo.VideoCapabilities used to infer support.
      */
    private static void addVp9CodecProfileLevels(CodecProfileLevelList profileLevels,
            MediaCodecInfo.CodecCapabilities codecCapabilities) {
        // https://www.webmproject.org/vp9/levels
        final int[][] bitrateMapping = {
                {200, 10}, {800, 11}, {1800, 20}, {3600, 21}, {7200, 30}, {12000, 31}, {18000, 40},
                {30000, 41}, {60000, 50}, {120000, 51}, {180000, 52},
        };
        VideoCapabilities videoCapabilities = codecCapabilities.getVideoCapabilities();
        for (int[] entry : bitrateMapping) {
            int bitrate = entry[0];
            int level = entry[1];
            if (videoCapabilities.getBitrateRange().contains(bitrate)) {
                // Assume all platforms before N only support VP9 profile 0.
                profileLevels.addCodecProfileLevel(
                        VideoCodec.VP9, VideoCodecProfile.VP9PROFILE_PROFILE0, level);
            }
        }
    }

    /**
      * Return an array of supported codecs and profiles.
      */
    @CalledByNative
    private static Object[] getSupportedCodecProfileLevels() {
        CodecProfileLevelList profileLevels = new CodecProfileLevelList();
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        for (MediaCodecInfo info : codecListHelper) {
            for (String mime : info.getSupportedTypes()) {
                if (!isDecoderSupportedForDevice(mime)) {
                    Log.w(TAG, "Decoder for type %s disabled on this device", mime);
                    continue;
                }

                // On versions L and M, VP9 codecCapabilities do not advertise profile level
                // support. In this case, estimate the level from MediaCodecInfo.VideoCapabilities
                // instead. Assume VP9 is not supported before L. For more information, consult
                // https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel.html
                try {
                    CodecCapabilities codecCapabilities = info.getCapabilitiesForType(mime);
                    if (mime.endsWith("vp9") && Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
                        addVp9CodecProfileLevels(profileLevels, codecCapabilities);
                        continue;
                    }
                    for (CodecProfileLevel profileLevel : codecCapabilities.profileLevels) {
                        profileLevels.addCodecProfileLevel(mime, profileLevel);
                    }
                } catch (IllegalArgumentException e) {
                    // Type is not supported.
                }
            }
        }
        return profileLevels.toArray();
    }

    /**
     * Creates MediaCodec decoder.
     * @param mime MIME type of the media.
     * @param codecType Type of codec to create.
     * @return CodecCreationInfo object
     */
    static CodecCreationInfo createDecoder(String mime, int codecType) {
        return createDecoder(mime,codecType,null);
    }

    /**
     * Creates MediaCodec decoder.
     * @param mime MIME type of the media.
     * @param codecType Type of codec to create.
     * @param mediaCrypto Crypto of the media.
     * @return CodecCreationInfo object
     */
    static CodecCreationInfo createDecoder(
            String mime, @CodecType int codecType, MediaCrypto mediaCrypto) {
        // Always return a valid CodecCreationInfo, its |mediaCodec| field will be null
        // if we cannot create the codec.

        CodecCreationInfo result = new CodecCreationInfo();

        assert result.mediaCodec == null;

        // Do not create codec for unsupported devices.
        if (!isDecoderSupportedForDevice(mime)) {
            Log.e(TAG, "Decoder for type %s is not supported on this device", mime);
            return result;
        }

        try {
            // "SECURE" only applies to video decoders.
            // Use MediaCrypto.requiresSecureDecoderComponent() for audio: crbug.com/727918
            if ((mime.startsWith("video") && codecType == CodecType.SECURE)
                    || (mime.startsWith("audio") && mediaCrypto != null
                               && mediaCrypto.requiresSecureDecoderComponent(mime))) {
                // Creating secure codecs is not supported directly on older
                // versions of Android. Therefore, always get the non-secure
                // codec name and append ".secure" to get the secure codec name.
                // TODO(xhwang): Now b/15587335 is fixed, we should have better
                // API support.
                String decoderName = getDefaultCodecName(mime, MediaCodecDirection.DECODER, false);
                if (decoderName.equals("")) return result;

                // To work around an issue that we cannot get the codec info
                // from the secure decoder, create an insecure decoder first
                // so that we can query its codec info. http://b/15587335.
                // Futhermore, it is impossible to create an insecure
                // decoder if the secure one is already created.
                MediaCodec insecureCodec = MediaCodec.createByCodecName(decoderName);
                result.supportsAdaptivePlayback =
                        codecSupportsAdaptivePlayback(insecureCodec, mime);
                insecureCodec.release();

                result.mediaCodec = MediaCodec.createByCodecName(decoderName + ".secure");
            } else {
                if (codecType == CodecType.SOFTWARE) {
                    String decoderName =
                            getDefaultCodecName(mime, MediaCodecDirection.DECODER, true);
                    result.mediaCodec = MediaCodec.createByCodecName(decoderName);
                } else if (mime.equals(MediaFormat.MIMETYPE_AUDIO_RAW)) {
                    result.mediaCodec = MediaCodec.createByCodecName("OMX.google.raw.decoder");
                } else {
                    result.mediaCodec = MediaCodec.createDecoderByType(mime);
                }
                result.supportsAdaptivePlayback =
                        codecSupportsAdaptivePlayback(result.mediaCodec, mime);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s, codecType: %d", mime, codecType, e);
            result.mediaCodec = null;
        }
        return result;
    }

    /**
     * This is a way to handle misbehaving devices.
     * Some devices cannot decode certain codecs, while other codecs work fine.
     *
     * Do not access MediaCodec or MediaCodecList in this function since it's
     * used from the renderer process.
     *
     * @param mime MIME type as passed to mediaCodec.createDecoderByType(mime).
     * @return true if this codec is supported for decoder on this device.
     */
    @CalledByNative
    static boolean isDecoderSupportedForDevice(String mime) {
        // *************************************************************
        // *** DO NOT ADD ANY NEW CODECS WITHOUT UPDATING MIME_UTIL. ***
        // *************************************************************
        if (mime.equals(MimeTypes.VIDEO_VP8)) {
            if (Build.MANUFACTURER.toLowerCase(Locale.getDefault()).equals("samsung")) {
                // Some Samsung devices cannot render VP8 video directly to the surface.

                // Samsung Galaxy S4 Mini.
                // Only GT-I9190 was tested with Android 4.4.2
                // We block it and the popular GT-I9195 for all Android versions.
                if (Build.MODEL.startsWith("GT-I9190") || Build.MODEL.startsWith("GT-I9195")) {
                    return false;
                }
            }

            // MediaTek decoders do not work properly on vp8. See http://crbug.com/446974 and
            // http://crbug.com/597836.
            if (Build.HARDWARE.startsWith("mt")) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return false;

                // The following chipsets have been confirmed by MediaTek to work on P+
                return Build.HARDWARE.startsWith("mt5599") || Build.HARDWARE.startsWith("mt5895")
                        || Build.HARDWARE.startsWith("mt8768")
                        || Build.HARDWARE.startsWith("mt5887");
            }
        } else if (mime.equals(MimeTypes.VIDEO_VP9)) {
            // Nexus Player VP9 decoder performs poorly at >= 1080p resolution.
            if (Build.MODEL.equals("Nexus Player")) {
                return false;
            }
        } else if (mime.equals(MimeTypes.VIDEO_AV1)) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false;
        } else if (mime.equals(MimeTypes.VIDEO_DOLBY_VISION)) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
                return false;
            }
        }
        // *************************************************************
        // *** DO NOT ADD ANY NEW CODECS WITHOUT UPDATING MIME_UTIL. ***
        // *************************************************************
        return true;
    }

    /**
     * Returns true if and only enabling adaptive playback is unsafe.  On some
     * device / os combinations, enabling it causes decoded frames to be
     * unusable.  For example, the S3 on 4.4.2 returns black and white, tiled
     * frames when this is enabled.
     */
    private static boolean isAdaptivePlaybackDenied(String mime) {
        if (!mime.equals("video/avc") && !mime.equals("video/avc1")) {
            return false;
        }

        if (!Build.VERSION.RELEASE.equals("4.4.2")) {
            return false;
        }

        if (!Build.MANUFACTURER.toLowerCase(Locale.getDefault()).equals("samsung")) {
            return false;
        }

        return Build.MODEL.startsWith("GT-I9300") || // S3 (I9300 / I9300I)
                Build.MODEL.startsWith("SCH-I535"); // S3
    }

    /**
     * Returns true if the given codec supports adaptive playback (dynamic resolution change).
     * @param mediaCodec the codec.
     * @param mime MIME type that corresponds to the codec creation.
     * @return true if this codec and mime type combination supports adaptive playback.
     */
    private static boolean codecSupportsAdaptivePlayback(MediaCodec mediaCodec, String mime) {
        if (mediaCodec == null) {
            return false;
        }
        try {
            MediaCodecInfo info = mediaCodec.getCodecInfo();
            if (info.isEncoder()) {
                return false;
            }

            if (isAdaptivePlaybackDenied(mime)) {
                return false;
            }

            MediaCodecInfo.CodecCapabilities capabilities = info.getCapabilitiesForType(mime);
            return (capabilities != null)
                    && capabilities.isFeatureSupported(
                               MediaCodecInfo.CodecCapabilities.FEATURE_AdaptivePlayback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot retrieve codec information", e);
        }
        return false;
    }

    // List of supported HW encoders.
    @IntDef({HWEncoder.QcomVp8, HWEncoder.QcomH264, HWEncoder.ExynosVp8, HWEncoder.ExynosVp9,
            HWEncoder.ExynosH264, HWEncoder.MediatekH264, HWEncoder.HisiH264})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HWEncoder {
        int QcomVp8 = 0;
        int QcomH264 = 1;
        int ExynosVp8 = 2;
        int ExynosVp9 = 3;
        int ExynosH264 = 4;
        int MediatekH264 = 5;
        int HisiH264 = 6;
        int NUM_ENTRIES = 7;
    }

    private static String getMimeForHWEncoder(@HWEncoder int decoder) {
        switch (decoder) {
            case HWEncoder.QcomVp8:
            case HWEncoder.ExynosVp8:
                return MimeTypes.VIDEO_VP8;
            case HWEncoder.ExynosVp9:
                return MimeTypes.VIDEO_VP9;
            case HWEncoder.QcomH264:
            case HWEncoder.ExynosH264:
            case HWEncoder.MediatekH264:
            case HWEncoder.HisiH264:
                return MimeTypes.VIDEO_H264;
        }
        return "";
    }

    private static String getPrefixForHWEncoder(@HWEncoder int decoder) {
        // NOTE: Prefixes must be lower case since the comparison is done in lower case.
        switch (decoder) {
            case HWEncoder.QcomVp8:
            case HWEncoder.QcomH264:
                return "qcom";
            case HWEncoder.ExynosVp8:
            case HWEncoder.ExynosVp9:
            case HWEncoder.ExynosH264:
                return "exynos";
            case HWEncoder.MediatekH264:
                return "mtk";
            case HWEncoder.HisiH264:
                return "hisi";
        }
        return "";
    }

    private static int getMinSDKForHWEncoder(@HWEncoder int decoder) {
        switch (decoder) {
            case HWEncoder.QcomVp8:
            case HWEncoder.QcomH264:
            case HWEncoder.ExynosH264:
                return Build.VERSION_CODES.LOLLIPOP;
            case HWEncoder.ExynosVp8:
            case HWEncoder.ExynosVp9:
                return Build.VERSION_CODES.M;
            case HWEncoder.MediatekH264:
                return Build.VERSION_CODES.O_MR1;
            case HWEncoder.HisiH264:
                return Build.VERSION_CODES.N;
        }
        return -1;
    }

    private static @BitrateAdjuster.Type int getBitrateAdjusterTypeForHWEncoder(
            @HWEncoder int decoder) {
        switch (decoder) {
            case HWEncoder.QcomVp8:
            case HWEncoder.QcomH264:
            case HWEncoder.ExynosVp8:
                return BitrateAdjuster.Type.NO_ADJUSTMENT;
            case HWEncoder.ExynosH264:
            case HWEncoder.MediatekH264:
            case HWEncoder.HisiH264:
                return BitrateAdjuster.Type.FRAMERATE_ADJUSTMENT;
        }
        throw new IllegalArgumentException("Invalid HWEncoder decoder parameter.");
    }

    /**
     * Creates MediaCodec encoder.
     * @param mime MIME type of the media.
     * @return CodecCreationInfo object
     */
    static CodecCreationInfo createEncoder(String mime) {
        // Always return a valid CodecCreationInfo, its |mediaCodec| field will be null
        // if we cannot create the codec.
        CodecCreationInfo result = new CodecCreationInfo();

        @Nullable
        @HWEncoder
        Integer encoderProperties = findHWEncoder(mime);
        if (encoderProperties == null) return result;

        try {
            result.mediaCodec = MediaCodec.createEncoderByType(mime);
            result.supportsAdaptivePlayback = false;
            result.bitrateAdjuster = getBitrateAdjusterTypeForHWEncoder(encoderProperties);
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s", mime, e);
        }
        return result;
    }

    /**
     * This is a way to handle misbehaving devices.
     * @param mime MIME type as passed to mediaCodec.createEncoderByType(mime).
     * @return true if this codec is supported for encoder on this device.
     */
    @CalledByNative
    static boolean isEncoderSupportedByDevice(String mime) {
        return findHWEncoder(mime) != null;
    }

    /**
     * Provides a way to avoid calling MediaCodec.setOutputSurface() on unsupported devices.
     * @return true if setOutputSurface() is expected to work.
     */
    @CalledByNative
    static boolean isSetOutputSurfaceSupported() {
        // All Huawei devices based on this processor will immediately hang during
        // MediaCodec.setOutputSurface().  http://crbug.com/683401
        // Huawei P9 lite will, eventually, get the decoder into a bad state if SetSurface is called
        // enough times (https://crbug.com/792261).
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                && !Build.HARDWARE.equalsIgnoreCase("hi6210sft")
                && !Build.HARDWARE.equalsIgnoreCase("hi6250");
    }

    /**
     * Find HW encoder with given MIME type.
     * @param mime MIME type of the media.
     * @return HWEncoder or null if not found.
     */
    private static @Nullable @HWEncoder Integer findHWEncoder(String mime) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        for (MediaCodecInfo info : codecListHelper) {
            if (!info.isEncoder() || isSoftwareCodec(info)) continue;

            String encoderName = null;
            for (String mimeType : info.getSupportedTypes()) {
                if (mimeType.equalsIgnoreCase(mime)) {
                    encoderName = info.getName().toLowerCase(Locale.getDefault());
                    break;
                }
            }

            if (encoderName == null) {
                continue; // No HW support in this codec; try the next one.
            }

            // Check if this is supported HW encoder.
            for (@HWEncoder int codecProperties = 0; codecProperties < HWEncoder.NUM_ENTRIES;
                    codecProperties++) {
                if (!mime.equalsIgnoreCase(getMimeForHWEncoder(codecProperties))) continue;

                String prefix = getPrefixForHWEncoder(codecProperties);
                if (encoderName.startsWith("omx." + prefix + ".")
                        || encoderName.startsWith("c2." + prefix + ".")) {
                    if (Build.VERSION.SDK_INT < getMinSDKForHWEncoder(codecProperties)) {
                        Log.w(TAG, "Codec " + encoderName + " is disabled due to SDK version "
                                        + Build.VERSION.SDK_INT);
                        continue;
                    }
                    Log.d(TAG, "Found target encoder for mime " + mime + " : " + encoderName);
                    return codecProperties;
                }
            }
        }

        Log.w(TAG, "HW encoder for " + mime + " is not available on this device.");
        return null;
    }

    /**
     * Returns true if and only if a platform with the given SDK API level supports the 'cbcs'
     * encryption scheme, specifically AES CBC encryption with possibility of pattern encryption.
     * While 'cbcs' scheme was originally implemented in N, there was a bug (in the
     * DRM code) which means that it didn't really work properly until N-MR1).
     */
    @CalledByNative
    static boolean platformSupportsCbcsEncryption(int sdk) {
        return sdk >= Build.VERSION_CODES.N_MR1;
    }

    /**
     * Sets the encryption pattern value if and only if CryptoInfo.setPattern method is
     * supported.
     * This method was introduced in Android N. Note that if platformSupportsCbcsEncryption
     * returns true, then this function will set the pattern.
     */
    static void setPatternIfSupported(CryptoInfo cryptoInfo, int encrypt, int skip) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            ApiHelperForN.setCryptoInfoPattern(cryptoInfo, encrypt, skip);
        }
    }
}
