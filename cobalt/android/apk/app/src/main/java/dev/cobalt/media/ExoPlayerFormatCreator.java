package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

// import androidx.annotation.OptIn;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.UnstableApi;
import dev.cobalt.util.Log;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public class ExoPlayerFormatCreator {

  static public Format createAudioFormat(String mime, byte[] audioConfigurationData, int sampleRate, int channelCount) {
    Format audioFormat = null;
    Log.i(TAG, String.format("Checking audio mime %s", mime));
    List<byte[]> csdsList = null;
    if (mime.equals("audio/opus")) {
      Log.i(TAG, "Creating opus audio format");
      if (audioConfigurationData != null) {
        Log.i(TAG, "Setting Opus config info in ExoPlayer");
        byte[][] csds = {};
        csds = MediaFormatBuilder.starboardParseOpusConfigurationData(sampleRate,
            audioConfigurationData);
        if (csds == null) {
          Log.e(TAG, "Error parsing Opus config info");
          assert (false);
        } else {
          csdsList = Arrays.asList(csds);
        }
      }
    }
      audioFormat = new Format.Builder()
          .setSampleMimeType(mime)
          .setSampleRate(sampleRate)
          .setChannelCount(channelCount)
          // .setCodecs("opus")
          .setPcmEncoding(C.ENCODING_PCM_FLOAT)
          .setInitializationData(csdsList)
          .build();
    // else if (mime.equals("audio/mp4a-latm")) {
    //   Log.i(TAG, "Creating AAC audio format");
    //   audioFormat = new Format.Builder()
    //       .setSampleMimeType(MimeTypes.AUDIO_AAC)
    //       .setSampleRate(sampleRate)
    //       .setChannelCount(channelCount)
    //       .setCodecs("aac")
    //       .setPcmEncoding(C.ENCODING_PCM_FLOAT)
    //       .build();
    // }
    // else {
    //   Log.i(TAG, String.format("Unsupported audio format %s", mime));
    // }
  return audioFormat;
  }

  static public Format createVideoFormat(String mime, int width, int height, float fps, int bitrate) {
    Format videoFormat = null;
    Format.Builder builder = new Format.Builder();
    Log.i(TAG, String.format("Creating video format for mime %s", mime));
    builder
        .setSampleMimeType(mime)
        .setWidth(width)
        .setHeight(height);
    if (bitrate > 0) {
      Log.i(TAG, String.format("Setting bitrate to %d", bitrate));
      builder.setAverageBitrate(bitrate);
    }

    // if (fps > 0.0) {
    //   Log.i(TAG, String.format("Setting fps to %f", fps));
    //   builder.setFrameRate(fps);
    // } else {
    //   Log.i(TAG, "Defaulting to 30 fps");
    //   builder.setFrameRate(30.0f);
    // }

    videoFormat = builder.build();
    return videoFormat;
  }

  // static public Format createFormat(String mimeType, boolean mustSupportSecure, boolean mustSupportHdr, boolean mustSupportTunnelMode, int frameHeight, int frameWidth, int bitrate, float fps) {
  //   String decoderInfo =
  //       String.format(
  //           Locale.US,
  //           "Searching for video decoder with parameters mimeType: %s, secure: %b, frameWidth:"
  //               + " %d, frameHeight: %d, bitrate: %d, fps: %f, mustSupportHdr: %b,"
  //               + " mustSupportTunnelMode: %b",
  //           mimeType,
  //           mustSupportSecure,
  //           frameWidth,
  //           frameHeight,
  //           bitrate,
  //           fps,
  //           mustSupportHdr,
  //           mustSupportTunnelMode);
  //   Log.v(TAG, decoderInfo);
  //
  //   Format.Builder builder = new Format.Builder();
  //   builder
  //       .setSampleMimeType(mimeType)
  //       .setAverageBitrate(bitrate)
  //       .setWidth(frameWidth)
  //       .setHeight(frameHeight);
  //
  // }

  // private MimeTypes mimeToMimeTypes(String mimeType) {
  //   switch (mimeType) {
  //     case "audio/mp4a-latm":
  //       return MimeTypes.AUDIO_AAC;
  //     case "audio/opus":
  //       return MimeTypes.AUDIO_OPUS;
  //     case "video/av01":
  //       return MimeTypes.VIDEO_AV1;
  //     case "video/avc":
  //       return MimeTypes.VIDEO_H264;
  //     case "video/x-vnd.on2.vp9":
  //       return MimeTypes.VIDEO_VP9;
  //
  //   }
  // }
}
