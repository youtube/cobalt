package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import androidx.media3.common.ColorInfo;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import dev.cobalt.util.Log;
import java.util.Arrays;
import java.util.Collections;
import org.chromium.base.annotations.CalledByNative;

/** Creates Formats for specified codec configurations */
public class ExoPlayerFormatCreator {

    public static Format createAudioFormat(
            int codec, String mime, byte[] audioConfigurationData, int sampleRate, int channelCount, int bitsPerSample) {
        Format.Builder builder = new Format.Builder()
            .setSampleRate(sampleRate)
            .setChannelCount(channelCount);

        String mimeType;
        switch(codec) {
            case ExoPlayerAudioCodec.AAC:
                mimeType = MimeTypes.AUDIO_AAC;
                if (audioConfigurationData != null) {
                    builder.setInitializationData(Collections.singletonList(audioConfigurationData));
                }
                break;
            case ExoPlayerAudioCodec.AC3:
                mimeType = MimeTypes.AUDIO_AC3;
                break;
            case ExoPlayerAudioCodec.EAC3:
                mimeType = MimeTypes.AUDIO_E_AC3;
                break;
            case ExoPlayerAudioCodec.OPUS:
                mimeType = MimeTypes.AUDIO_OPUS;
                if (audioConfigurationData == null) {
                    Log.e(TAG, "Opus stream is missing configuration data.");
                    return null;
                }

                    byte[][] csds = {};
                    csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                        sampleRate, audioConfigurationData);
                    if (csds == null) {
                        Log.e(TAG, "Error parsing Opus config info");
                        return null;
                    } else {
                        builder.setInitializationData(Arrays.asList(csds));
                    }
                break;
            case ExoPlayerAudioCodec.VORBIS:
                mimeType = MimeTypes.AUDIO_VORBIS;
                break;
            case ExoPlayerAudioCodec.MP3:
                mimeType = MimeTypes.AUDIO_MPEG;
                break;
            case ExoPlayerAudioCodec.FLAC:
                mimeType = MimeTypes.AUDIO_FLAC;
                break;
            case ExoPlayerAudioCodec.PCM:
                mimeType = MimeTypes.AUDIO_RAW;
                break;
            default:
                Log.e(TAG, String.format("Unsupported audio encountered in createAudioFormat, value: %d", codec));
                return null;
        }

        builder.setSampleMimeType(mimeType);

        return builder.build();
    }

    public static Format createVideoFormat(
            String mime, int width, int height, float fps, int bitrate, ColorInfo colorInfo) {
        Format.Builder builder = new Format.Builder();
        builder.setSampleMimeType(mime).setWidth(width).setHeight(height);
        if (bitrate > 0) {
            builder.setAverageBitrate(bitrate);
        }

        if (fps > 0) {
            builder.setFrameRate(fps);
        }

        if (colorInfo != null) {
            builder.setColorInfo(colorInfo);
        }

        return builder.build();
    }

    @CalledByNative
    public static ColorInfo createColorInfo(int colorRange,
        int colorSpace,
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
        int maxFall) {

        MediaCodecBridge.ColorInfo info = new MediaCodecBridge.ColorInfo(colorRange, colorSpace, colorTransfer, primaryRChromaticityX, primaryRChromaticityY, primaryGChromaticityX, primaryGChromaticityY, primaryBChromaticityX, primaryBChromaticityY, whitePointChromaticityX, whitePointChromaticityY, maxMasteringLuminance, minMasteringLuminance, maxCll, maxFall, false);
    return new ColorInfo.Builder().setColorRange(info.colorRange).setColorSpace(info.colorStandard).setColorTransfer(info.colorTransfer).setHdrStaticInfo(info.hdrStaticInfo.array()).build();

    }
}
