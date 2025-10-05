package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.media.MediaFormat;
import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.UnstableApi;

import com.google.common.collect.ImmutableList;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

import dev.cobalt.util.Log;

public class ExoPlayerFormatCreator {

    static private int bitDepthToEncoding(int bitsPerSample) {
        switch (bitsPerSample) {
            case 16:
                return C.ENCODING_PCM_16BIT;
            case 32:
                return C.ENCODING_PCM_FLOAT;
            default:
                Log.i(TAG, String.format("Unsupported audio bits per sample %d", bitsPerSample));
                return -1;
        }
    }

    //  static private void setCodecSpecificData(byte[][] csds) {
    //     // Codec Specific Data is set in the MediaFormat as ByteBuffer entries with keys csd-0,
    //     // csd-1, and so on. See:
    //     // http://developer.android.com/reference/android/media/MediaCodec.html for details.
    //     for (int i = 0; i < csds.length; ++i) {
    //         if (csds[i].length == 0) {
    //             continue;
    //         }
    //         String name = "csd-" + i;
    //         format.setByteBuffer(name, ByteBuffer.wrap(csds[i]));
    //     }
    // }

    static public Format createAudioFormat(
            String mime, byte[] audioConfigurationData, int sampleRate, int channelCount, int bitsPerSample) {
        Log.i(TAG, String.format("Creating audio format for %s with sample rate %d, channel count %d, configuration data? %s", mime, sampleRate, channelCount, (audioConfigurationData == null ? "no" : "yes")));

        int pcmEncoding = bitDepthToEncoding(bitsPerSample);
        if (pcmEncoding == -1) {
            return null;
        }
        Format.Builder audioFormatBuilder = new Format.Builder()
            .setSampleRate(sampleRate)
            .setChannelCount(channelCount);
        List<byte[]> csdsList = null;
        if (mime.equals("audio/opus")) {
            Log.i(TAG, "Creating opus audio format");
            audioFormatBuilder = audioFormatBuilder.setSampleMimeType(MimeTypes.AUDIO_OPUS).setPcmEncoding(C.ENCODING_PCM_FLOAT);
            if (audioConfigurationData != null) {
                byte[][] csds = {};
                csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                        sampleRate, audioConfigurationData);
                if (csds == null) {
                    Log.e(TAG, "Error parsing Opus config info");
                    return null;
                } else {
                    csdsList = Arrays.asList(csds);
                    audioFormatBuilder = audioFormatBuilder.setInitializationData(csdsList);
                }
            }
        } else {
            audioFormatBuilder = audioFormatBuilder.setSampleMimeType(MimeTypes.AUDIO_AAC).setCodecs("mp4a.40.2");
            if (audioConfigurationData != null) {
                Log.i(TAG, String.format("SETTING CONFIGURATION DATA FOR AAC, length: %d", audioConfigurationData.length));
                audioFormatBuilder = audioFormatBuilder.setInitializationData(Collections.singletonList(audioConfigurationData));
            }
        }

        return audioFormatBuilder.build();
    }

    static public Format createVideoFormat(
            String mime, int width, int height, float fps, int bitrate) {
        Format videoFormat = null;
        Format.Builder builder = new Format.Builder();
        builder.setSampleMimeType(mime).setWidth(width).setHeight(height);
        if (bitrate > 0) {
            builder.setAverageBitrate(bitrate);
        }

        videoFormat = builder.build();
        return videoFormat;
    }
}
