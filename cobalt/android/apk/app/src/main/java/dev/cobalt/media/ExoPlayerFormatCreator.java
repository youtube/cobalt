package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

// import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.UnstableApi;
import dev.cobalt.util.Log;
import java.util.List;

public class ExoPlayerFormatCreator {

  static public Format createAudioFormat(String mime, byte[] audioConfigurationData, int sampleRate, int channelCount) {
    Format audioFormat = null;
    if (mime.equals("audio/opus")) {
      Log.i(TAG, "Creating opus audio format");
      List<byte[]> csdsList = null;
      if (audioConfigurationData != null) {
        Log.i(TAG, "Setting Opus config info in ExoPlayer");
        byte[][] csds = {};
        csds = MediaFormatBuilder.starboardParseOpusConfigurationData(sampleRate,
            audioConfigurationData);
        if (csds == null) {
          Log.e(TAG, "Error parsing Opus config info");
          assert (false);
        }
      }
      audioFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.AUDIO_OPUS)
          .setSampleRate(sampleRate)
          .setChannelCount(channelCount)
          .setCodecs("opus")
          .setPcmEncoding(C.ENCODING_PCM_FLOAT)
          .setInitializationData(csdsList)
          .build();
    } else if (mime.equals("audio/mp4a-latm")) {
      Log.i(TAG, "Creating AAC audio format");
      audioFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.AUDIO_AAC)
          .setSampleRate(sampleRate)
          .setChannelCount(channelCount)
          .setCodecs("aac")
          .setPcmEncoding(C.ENCODING_PCM_FLOAT)
          .build();
    } else {
      Log.i(TAG, String.format("Unsupported audio format %s", mime));
    }
  return audioFormat;
  }

  static public Format createVideoFormat(String mime, int width, int height, float fps, int bitrate) {
    Format videoFormat = null;
    if (mime.equals("video/avc")) {
      Log.i(TAG, "Creating H264 video format");
      videoFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.VIDEO_H264)
          .setAverageBitrate(bitrate)
          .setWidth(width)
          .setHeight(height)
          .setFrameRate(fps)
          .build();
    } else if (mime.equals("video/x-vnd.on2.vp9")) {
      Log.i(TAG, "Creating VP9 video format");
      videoFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.VIDEO_VP9)
          .setAverageBitrate(bitrate)
          .setWidth(width)
          .setHeight(height)
          .setFrameRate(fps)
          .build();
    } else {
      Log.i(TAG, String.format("Unsupported video format %s", mime));
    }
    return videoFormat;
  }
}
