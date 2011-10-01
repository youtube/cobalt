// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/ffmpeg_common.h"

#include "base/logging.h"

namespace media {

static const AVRational kMicrosBase = { 1, base::Time::kMicrosecondsPerSecond };

base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                    int64 timestamp) {
  int64 microseconds = av_rescale_q(timestamp, time_base, kMicrosBase);
  return base::TimeDelta::FromMicroseconds(microseconds);
}

int64 ConvertToTimeBase(const AVRational& time_base,
                        const base::TimeDelta& timestamp) {
  return av_rescale_q(timestamp.InMicroseconds(), kMicrosBase, time_base);
}

static AudioCodec CodecIDToAudioCodec(CodecID codec_id) {
  switch (codec_id) {
    case CODEC_ID_AAC:
      return kCodecAAC;
    case CODEC_ID_MP3:
      return kCodecMP3;
    case CODEC_ID_VORBIS:
      return kCodecVorbis;
    case CODEC_ID_PCM_U8:
    case CODEC_ID_PCM_S16LE:
    case CODEC_ID_PCM_S32LE:
      return kCodecPCM;
    default:
      NOTREACHED();
  }
  return kUnknownAudioCodec;
}

static CodecID AudioCodecToCodecID(AudioCodec audio_codec,
                                   int bits_per_channel) {
  switch (audio_codec) {
    case kUnknownAudioCodec:
      return CODEC_ID_NONE;
    case kCodecAAC:
      return CODEC_ID_AAC;
    case kCodecMP3:
      return CODEC_ID_MP3;
    case kCodecPCM:
      switch (bits_per_channel) {
        case 8:
          return CODEC_ID_PCM_U8;
        case 16:
          return CODEC_ID_PCM_S16LE;
        case 32:
          return CODEC_ID_PCM_S32LE;
        default:
          NOTREACHED() << "Unsupported bits_per_channel: " << bits_per_channel;
      }
    case kCodecVorbis:
      return CODEC_ID_VORBIS;
    default:
      NOTREACHED();
  }
  return CODEC_ID_NONE;
}

void AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    AudioDecoderConfig* config) {
  DCHECK_EQ(codec_context->codec_type, AVMEDIA_TYPE_AUDIO);

  AudioCodec codec = CodecIDToAudioCodec(codec_context->codec_id);
  int bits_per_channel = av_get_bits_per_sample_fmt(codec_context->sample_fmt);
  ChannelLayout channel_layout =
      ChannelLayoutToChromeChannelLayout(codec_context->channel_layout,
                                         codec_context->channels);
  int samples_per_second = codec_context->sample_rate;

  config->Initialize(codec,
                     bits_per_channel,
                     channel_layout,
                     samples_per_second,
                     codec_context->extradata,
                     codec_context->extradata_size);
}

void AudioDecoderConfigToAVCodecContext(const AudioDecoderConfig& config,
                                        AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = AudioCodecToCodecID(config.codec(),
                                                config.bits_per_channel());

  switch (config.bits_per_channel()) {
    case 8:
      codec_context->sample_fmt = AV_SAMPLE_FMT_U8;
      break;
    case 16:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
      break;
    case 32:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S32;
      break;
    default:
      NOTIMPLEMENTED() << "TODO(scherkus): DO SOMETHING BETTER HERE?";
      codec_context->sample_fmt = AV_SAMPLE_FMT_NONE;
  }

  // TODO(scherkus): should we set |channel_layout|? I'm not sure if FFmpeg uses
  // said information to decode.
  codec_context->channels =
      ChannelLayoutToChannelCount(config.channel_layout());
  codec_context->sample_rate = config.samples_per_second();

  if (config.extra_data()) {
    codec_context->extradata_size = config.extra_data_size();
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, config.extra_data(),
           config.extra_data_size());
    memset(codec_context->extradata + config.extra_data_size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context->extradata = NULL;
    codec_context->extradata_size = 0;
  }
}

VideoCodec CodecIDToVideoCodec(CodecID codec_id) {
  switch (codec_id) {
    case CODEC_ID_VC1:
      return kCodecVC1;
    case CODEC_ID_H264:
      return kCodecH264;
    case CODEC_ID_THEORA:
      return kCodecTheora;
    case CODEC_ID_MPEG2VIDEO:
      return kCodecMPEG2;
    case CODEC_ID_MPEG4:
      return kCodecMPEG4;
    case CODEC_ID_VP8:
      return kCodecVP8;
    default:
      NOTREACHED();
  }
  return kUnknownVideoCodec;
}

CodecID VideoCodecToCodecID(VideoCodec video_codec) {
  switch (video_codec) {
    case kUnknownVideoCodec:
      return CODEC_ID_NONE;
    case kCodecVC1:
      return CODEC_ID_VC1;
    case kCodecH264:
      return CODEC_ID_H264;
    case kCodecTheora:
      return CODEC_ID_THEORA;
    case kCodecMPEG2:
      return CODEC_ID_MPEG2VIDEO;
    case kCodecMPEG4:
      return CODEC_ID_MPEG4;
    case kCodecVP8:
      return CODEC_ID_VP8;
    default:
      NOTREACHED();
  }
  return CODEC_ID_NONE;
}

ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                 int channels) {
  switch (layout) {
    case CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4POINT0;
    case CH_LAYOUT_2_2:
      return CHANNEL_LAYOUT_2_2;
    case CH_LAYOUT_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case CH_LAYOUT_5POINT0:
      return CHANNEL_LAYOUT_5POINT0;
    case CH_LAYOUT_5POINT1:
      return CHANNEL_LAYOUT_5POINT1;
    case CH_LAYOUT_5POINT0_BACK:
      return CHANNEL_LAYOUT_5POINT0_BACK;
    case CH_LAYOUT_5POINT1_BACK:
      return CHANNEL_LAYOUT_5POINT1_BACK;
    case CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7POINT0;
    case CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7POINT1;
    case CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7POINT1_WIDE;
    case CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    default:
      // FFmpeg channel_layout is 0 for .wav and .mp3.  We know mono and stereo
      // from the number of channels, otherwise report errors.
      if (channels == 1)
        return CHANNEL_LAYOUT_MONO;
      if (channels == 2)
        return CHANNEL_LAYOUT_STEREO;
      DLOG(WARNING) << "Unsupported/unencountered channel layout values";
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

base::TimeDelta GetFrameDuration(AVStream* stream) {
  AVRational time_base = { stream->r_frame_rate.den, stream->r_frame_rate.num };
  return ConvertFromTimeBase(time_base, 1);
}

gfx::Size GetNaturalSize(AVStream* stream) {
  double aspect_ratio = 1.0;

  if (stream->sample_aspect_ratio.num)
    aspect_ratio = av_q2d(stream->sample_aspect_ratio);
  else if (stream->codec->sample_aspect_ratio.num)
    aspect_ratio = av_q2d(stream->codec->sample_aspect_ratio);

  int height = stream->codec->height;
  int width = floor(stream->codec->width * aspect_ratio + 0.5);

  // An even width makes things easier for YV12 and appears to be the behavior
  // expected by WebKit layout tests.
  return gfx::Size(width & ~1, height);
}

void DestroyAVFormatContext(AVFormatContext* format_context) {
  DCHECK(format_context);

  // Iterate each stream and destroy each one of them.
  if (format_context->streams) {
    int streams = format_context->nb_streams;
    for (int i = 0; i < streams; ++i) {
      AVStream* stream = format_context->streams[i];

      // The conditions for calling avcodec_close():
      // 1. AVStream is alive.
      // 2. AVCodecContext in AVStream is alive.
      // 3. AVCodec in AVCodecContext is alive.
      // Notice that closing a codec context without prior avcodec_open() will
      // result in a crash in FFmpeg.
      if (stream && stream->codec && stream->codec->codec) {
        stream->discard = AVDISCARD_ALL;
        avcodec_close(stream->codec);
      }
    }
  }

  // Then finally cleanup the format context.
  av_close_input_file(format_context);
}

}  // namespace media
