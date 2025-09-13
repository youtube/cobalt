package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.UnstableApi;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;

import dev.cobalt.util.Log;

public class ExoPlayerFormatCreator {
    static public Format createAudioFormat(
            String mime, byte[] audioConfigurationData, int sampleRate, int channelCount) {
        Format audioFormat = null;
        List<byte[]> csdsList = null;
        if (mime.equals("audio/opus")) {
            Log.i(TAG, "Creating opus audio format");
            if (audioConfigurationData != null) {
                byte[][] csds = {};
                csds = MediaFormatBuilder.starboardParseOpusConfigurationData(
                        sampleRate, audioConfigurationData);
                if (csds == null) {
                    Log.e(TAG, "Error parsing Opus config info");
                    return audioFormat;
                } else {
                    csdsList = Arrays.asList(csds);
                }
            }
        }

        audioFormat = new Format.Builder()
                              .setSampleMimeType(mime)
                              .setSampleRate(sampleRate)
                              .setChannelCount(channelCount)
                              .setPcmEncoding(C.ENCODING_PCM_FLOAT)
                              .setInitializationData(csdsList)
                              .build();
        return audioFormat;
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
