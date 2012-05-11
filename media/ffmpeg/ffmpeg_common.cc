// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

AudioCodec CodecIDToAudioCodec(CodecID codec_id) {
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
    case CODEC_ID_FLAC:
      return kCodecFLAC;
    case CODEC_ID_AMR_NB:
      return kCodecAMR_NB;
    case CODEC_ID_AMR_WB:
      return kCodecAMR_WB;
    case CODEC_ID_PCM_MULAW:
      return kCodecPCM_MULAW;
    default:
      DVLOG(1) << "Unknown audio CodecID: " << codec_id;
  }
  return kUnknownAudioCodec;
}

static CodecID AudioCodecToCodecID(AudioCodec audio_codec,
                                   int bits_per_channel) {
  switch (audio_codec) {
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
          DVLOG(1) << "Unsupported bits per channel: " << bits_per_channel;
      }
      break;
    case kCodecVorbis:
      return CODEC_ID_VORBIS;
    case kCodecFLAC:
      return CODEC_ID_FLAC;
    case kCodecAMR_NB:
      return CODEC_ID_AMR_NB;
    case kCodecAMR_WB:
      return CODEC_ID_AMR_WB;
    case kCodecPCM_MULAW:
      return CODEC_ID_PCM_MULAW;
    default:
      DVLOG(1) << "Unknown AudioCodec: " << audio_codec;
  }
  return CODEC_ID_NONE;
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
      DVLOG(1) << "Unknown video CodecID: " << codec_id;
  }
  return kUnknownVideoCodec;
}

static CodecID VideoCodecToCodecID(VideoCodec video_codec) {
  switch (video_codec) {
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
      DVLOG(1) << "Unknown VideoCodec: " << video_codec;
  }
  return CODEC_ID_NONE;
}

static VideoCodecProfile ProfileIDToVideoCodecProfile(int profile) {
  // Clear out the CONSTRAINED & INTRA flags which are strict subsets of the
  // corresponding profiles with which they're used.
  profile &= ~FF_PROFILE_H264_CONSTRAINED;
  profile &= ~FF_PROFILE_H264_INTRA;
  switch (profile) {
    case FF_PROFILE_H264_BASELINE:
      return H264PROFILE_BASELINE;
    case FF_PROFILE_H264_MAIN:
      return H264PROFILE_MAIN;
    case FF_PROFILE_H264_EXTENDED:
      return H264PROFILE_EXTENDED;
    case FF_PROFILE_H264_HIGH:
      return H264PROFILE_HIGH;
    case FF_PROFILE_H264_HIGH_10:
      return H264PROFILE_HIGH10PROFILE;
    case FF_PROFILE_H264_HIGH_422:
      return H264PROFILE_HIGH422PROFILE;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      return H264PROFILE_HIGH444PREDICTIVEPROFILE;
    default:
      DVLOG(1) << "Unknown profile id: " << profile;
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

static int VideoCodecProfileToProfileID(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return FF_PROFILE_H264_BASELINE;
    case H264PROFILE_MAIN:
      return FF_PROFILE_H264_MAIN;
    case H264PROFILE_EXTENDED:
      return FF_PROFILE_H264_EXTENDED;
    case H264PROFILE_HIGH:
      return FF_PROFILE_H264_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return FF_PROFILE_H264_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return FF_PROFILE_H264_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return FF_PROFILE_H264_HIGH_444_PREDICTIVE;
    default:
      DVLOG(1) << "Unknown VideoCodecProfile: " << profile;
  }
  return FF_PROFILE_UNKNOWN;
}

void AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    AudioDecoderConfig* config) {
  DCHECK_EQ(codec_context->codec_type, AVMEDIA_TYPE_AUDIO);

  AudioCodec codec = CodecIDToAudioCodec(codec_context->codec_id);
  int bytes_per_channel = av_get_bytes_per_sample(codec_context->sample_fmt);
  ChannelLayout channel_layout =
      ChannelLayoutToChromeChannelLayout(codec_context->channel_layout,
                                         codec_context->channels);
  int samples_per_second = codec_context->sample_rate;

  config->Initialize(codec,
                     bytes_per_channel << 3,
                     channel_layout,
                     samples_per_second,
                     codec_context->extradata,
                     codec_context->extradata_size,
                     true);
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
      DVLOG(1) << "Unsupported bits per channel: " << config.bits_per_channel();
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

void AVStreamToVideoDecoderConfig(
    const AVStream* stream,
    VideoDecoderConfig* config) {
  gfx::Size coded_size(stream->codec->coded_width, stream->codec->coded_height);

  // TODO(vrk): This assumes decoded frame data starts at (0, 0), which is true
  // for now, but may not always be true forever. Fix this in the future.
  gfx::Rect visible_rect(stream->codec->width, stream->codec->height);

  AVRational aspect_ratio = { 1, 1 };
  if (stream->sample_aspect_ratio.num)
    aspect_ratio = stream->sample_aspect_ratio;
  else if (stream->codec->sample_aspect_ratio.num)
    aspect_ratio = stream->codec->sample_aspect_ratio;

  config->Initialize(CodecIDToVideoCodec(stream->codec->codec_id),
                     ProfileIDToVideoCodecProfile(stream->codec->profile),
                     PixelFormatToVideoFormat(stream->codec->pix_fmt),
                     coded_size, visible_rect,
                     stream->r_frame_rate.num,
                     stream->r_frame_rate.den,
                     aspect_ratio.num,
                     aspect_ratio.den,
                     stream->codec->extradata,
                     stream->codec->extradata_size,
                     true);
}

void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context->codec_id = VideoCodecToCodecID(config.codec());
  codec_context->profile = VideoCodecProfileToProfileID(config.profile());
  codec_context->coded_width = config.coded_size().width();
  codec_context->coded_height = config.coded_size().height();
  codec_context->pix_fmt = VideoFormatToPixelFormat(config.format());

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

ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                 int channels) {
  switch (layout) {
    case AV_CH_LAYOUT_MONO:
      return CHANNEL_LAYOUT_MONO;
    case AV_CH_LAYOUT_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case AV_CH_LAYOUT_2_1:
      return CHANNEL_LAYOUT_2_1;
    case AV_CH_LAYOUT_SURROUND:
      return CHANNEL_LAYOUT_SURROUND;
    case AV_CH_LAYOUT_4POINT0:
      return CHANNEL_LAYOUT_4_0;
    case AV_CH_LAYOUT_2_2:
      return CHANNEL_LAYOUT_2_2;
    case AV_CH_LAYOUT_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case AV_CH_LAYOUT_5POINT0:
      return CHANNEL_LAYOUT_5_0;
    case AV_CH_LAYOUT_5POINT1:
      return CHANNEL_LAYOUT_5_1;
    case AV_CH_LAYOUT_5POINT0_BACK:
      return CHANNEL_LAYOUT_5_0_BACK;
    case AV_CH_LAYOUT_5POINT1_BACK:
      return CHANNEL_LAYOUT_5_1_BACK;
    case AV_CH_LAYOUT_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case AV_CH_LAYOUT_7POINT1:
      return CHANNEL_LAYOUT_7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return CHANNEL_LAYOUT_7_1_WIDE;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return CHANNEL_LAYOUT_STEREO_DOWNMIX;
    default:
      // FFmpeg channel_layout is 0 for .wav and .mp3.  We know mono and stereo
      // from the number of channels, otherwise report errors.
      if (channels == 1)
        return CHANNEL_LAYOUT_MONO;
      if (channels == 2)
        return CHANNEL_LAYOUT_STEREO;
      DVLOG(1) << "Unsupported channel layout: " << layout;
  }
  return CHANNEL_LAYOUT_UNSUPPORTED;
}

VideoFrame::Format PixelFormatToVideoFormat(PixelFormat pixel_format) {
  switch (pixel_format) {
    case PIX_FMT_YUV422P:
      return VideoFrame::YV16;
    case PIX_FMT_YUV420P:
      return VideoFrame::YV12;
    default:
      DVLOG(1) << "Unsupported PixelFormat: " << pixel_format;
  }
  return VideoFrame::INVALID;
}

PixelFormat VideoFormatToPixelFormat(VideoFrame::Format video_format) {
  switch (video_format) {
    case VideoFrame::YV16:
      return PIX_FMT_YUV422P;
    case VideoFrame::YV12:
      return PIX_FMT_YUV420P;
    default:
      DVLOG(1) << "Unsupported VideoFrame::Format: " << video_format;
  }
  return PIX_FMT_NONE;
}

base::TimeDelta GetFrameDuration(const VideoDecoderConfig& config) {
  AVRational time_base = {
    config.frame_rate_denominator(),
    config.frame_rate_numerator()
  };
  return ConvertFromTimeBase(time_base, 1);
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
      // Notice that closing a codec context without prior avcodec_open2() will
      // result in a crash in FFmpeg.
      if (stream && stream->codec && stream->codec->codec) {
        stream->discard = AVDISCARD_ALL;
        avcodec_close(stream->codec);
      }
    }
  }

  // Then finally cleanup the format context.
  avformat_close_input(&format_context);
}

}  // namespace media
