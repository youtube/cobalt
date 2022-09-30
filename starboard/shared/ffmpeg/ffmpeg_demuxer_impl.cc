// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/ffmpeg/ffmpeg_demuxer_impl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

constexpr int64_t kNoFFmpegTimestamp = static_cast<int64_t>(AV_NOPTS_VALUE);
constexpr int kAvioBufferSize = 4 * 1024;  // 4KB.

CobaltExtensionDemuxerAudioCodec AvCodecIdToAudioCodec(AVCodecID codec) {
  switch (codec) {
    case AV_CODEC_ID_AAC:
      return kCobaltExtensionDemuxerCodecAAC;
    case AV_CODEC_ID_MP3:
      return kCobaltExtensionDemuxerCodecMP3;
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_F32LE:
      return kCobaltExtensionDemuxerCodecPCM;
    case AV_CODEC_ID_VORBIS:
      return kCobaltExtensionDemuxerCodecVorbis;
    case AV_CODEC_ID_FLAC:
      return kCobaltExtensionDemuxerCodecFLAC;
    case AV_CODEC_ID_AMR_NB:
      return kCobaltExtensionDemuxerCodecAMR_NB;
    case AV_CODEC_ID_AMR_WB:
      return kCobaltExtensionDemuxerCodecAMR_WB;
    case AV_CODEC_ID_PCM_MULAW:
      return kCobaltExtensionDemuxerCodecPCM_MULAW;
    case AV_CODEC_ID_PCM_S16BE:
      return kCobaltExtensionDemuxerCodecPCM_S16BE;
    case AV_CODEC_ID_PCM_S24BE:
      return kCobaltExtensionDemuxerCodecPCM_S24BE;
    case AV_CODEC_ID_OPUS:
      return kCobaltExtensionDemuxerCodecOpus;
    case AV_CODEC_ID_EAC3:
      return kCobaltExtensionDemuxerCodecEAC3;
    case AV_CODEC_ID_PCM_ALAW:
      return kCobaltExtensionDemuxerCodecPCM_ALAW;
    case AV_CODEC_ID_ALAC:
      return kCobaltExtensionDemuxerCodecALAC;
    case AV_CODEC_ID_AC3:
      return kCobaltExtensionDemuxerCodecAC3;
    default:
      return kCobaltExtensionDemuxerCodecUnknownAudio;
  }
}

CobaltExtensionDemuxerVideoCodec AvCodecIdToVideoCodec(AVCodecID codec) {
  switch (codec) {
    case AV_CODEC_ID_H264:
      return kCobaltExtensionDemuxerCodecH264;
    case AV_CODEC_ID_VC1:
      return kCobaltExtensionDemuxerCodecVC1;
    case AV_CODEC_ID_MPEG2VIDEO:
      return kCobaltExtensionDemuxerCodecMPEG2;
    case AV_CODEC_ID_MPEG4:
      return kCobaltExtensionDemuxerCodecMPEG4;
    case AV_CODEC_ID_THEORA:
      return kCobaltExtensionDemuxerCodecTheora;
    case AV_CODEC_ID_VP8:
      return kCobaltExtensionDemuxerCodecVP8;
#if FFMPEG >= 560
    case AV_CODEC_ID_VP9:
      return kCobaltExtensionDemuxerCodecVP9;
    case AV_CODEC_ID_HEVC:
      return kCobaltExtensionDemuxerCodecHEVC;
#endif  // FFMPEG >= 560
    default:
      return kCobaltExtensionDemuxerCodecUnknownVideo;
  }
}

CobaltExtensionDemuxerSampleFormat AvSampleFormatToSampleFormat(
    AVSampleFormat sample_format) {
  switch (sample_format) {
    case AV_SAMPLE_FMT_U8:
      return kCobaltExtensionDemuxerSampleFormatU8;
    case AV_SAMPLE_FMT_S16:
      return kCobaltExtensionDemuxerSampleFormatS16;
    case AV_SAMPLE_FMT_S32:
      return kCobaltExtensionDemuxerSampleFormatS32;
    case AV_SAMPLE_FMT_FLT:
      return kCobaltExtensionDemuxerSampleFormatF32;
    case AV_SAMPLE_FMT_S16P:
      return kCobaltExtensionDemuxerSampleFormatPlanarS16;
    case AV_SAMPLE_FMT_S32P:
      return kCobaltExtensionDemuxerSampleFormatPlanarS32;
    case AV_SAMPLE_FMT_FLTP:
      return kCobaltExtensionDemuxerSampleFormatPlanarF32;
    default:
      return kCobaltExtensionDemuxerSampleFormatUnknown;
  }
}

CobaltExtensionDemuxerChannelLayout GuessChannelLayout(int channels) {
  switch (channels) {
    case 1:
      return kCobaltExtensionDemuxerChannelLayoutMono;
    case 2:
      return kCobaltExtensionDemuxerChannelLayoutStereo;
    case 3:
      return kCobaltExtensionDemuxerChannelLayoutSurround;
    case 4:
      return kCobaltExtensionDemuxerChannelLayoutQuad;
    case 5:
      return kCobaltExtensionDemuxerChannelLayout5_0;
    case 6:
      return kCobaltExtensionDemuxerChannelLayout5_1;
    case 7:
      return kCobaltExtensionDemuxerChannelLayout6_1;
    case 8:
      return kCobaltExtensionDemuxerChannelLayout7_1;
    default:
      SB_LOG(ERROR) << "Unsupported channel count: " << channels;
  }
  return kCobaltExtensionDemuxerChannelLayoutUnsupported;
}

CobaltExtensionDemuxerChannelLayout AvChannelLayoutToChannelLayout(
    uint64_t channel_layout,
    int num_channels) {
  if (num_channels > 8) {
    return kCobaltExtensionDemuxerChannelLayoutDiscrete;
  }

  switch (channel_layout) {
    case AV_CH_LAYOUT_MONO:
      return kCobaltExtensionDemuxerChannelLayoutMono;
    case AV_CH_LAYOUT_STEREO:
      return kCobaltExtensionDemuxerChannelLayoutStereo;
    case AV_CH_LAYOUT_2_1:
      return kCobaltExtensionDemuxerChannelLayout2_1;
    case AV_CH_LAYOUT_SURROUND:
      return kCobaltExtensionDemuxerChannelLayoutSurround;
    case AV_CH_LAYOUT_4POINT0:
      return kCobaltExtensionDemuxerChannelLayout4_0;
    case AV_CH_LAYOUT_2_2:
      return kCobaltExtensionDemuxerChannelLayout2_2;
    case AV_CH_LAYOUT_QUAD:
      return kCobaltExtensionDemuxerChannelLayoutQuad;
    case AV_CH_LAYOUT_5POINT0:
      return kCobaltExtensionDemuxerChannelLayout5_0;
    case AV_CH_LAYOUT_5POINT1:
      return kCobaltExtensionDemuxerChannelLayout5_1;
    case AV_CH_LAYOUT_5POINT0_BACK:
      return kCobaltExtensionDemuxerChannelLayout5_0Back;
    case AV_CH_LAYOUT_5POINT1_BACK:
      return kCobaltExtensionDemuxerChannelLayout5_1Back;
    case AV_CH_LAYOUT_7POINT0:
      return kCobaltExtensionDemuxerChannelLayout7_0;
    case AV_CH_LAYOUT_7POINT1:
      return kCobaltExtensionDemuxerChannelLayout7_1;
    case AV_CH_LAYOUT_7POINT1_WIDE:
      return kCobaltExtensionDemuxerChannelLayout7_1Wide;
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
      return kCobaltExtensionDemuxerChannelLayoutStereoDownmix;
    case AV_CH_LAYOUT_2POINT1:
      return kCobaltExtensionDemuxerChannelLayout2point1;
    case AV_CH_LAYOUT_3POINT1:
      return kCobaltExtensionDemuxerChannelLayout3_1;
    case AV_CH_LAYOUT_4POINT1:
      return kCobaltExtensionDemuxerChannelLayout4_1;
    case AV_CH_LAYOUT_6POINT0:
      return kCobaltExtensionDemuxerChannelLayout6_0;
    case AV_CH_LAYOUT_6POINT0_FRONT:
      return kCobaltExtensionDemuxerChannelLayout6_0Front;
    case AV_CH_LAYOUT_HEXAGONAL:
      return kCobaltExtensionDemuxerChannelLayoutHexagonal;
    case AV_CH_LAYOUT_6POINT1:
      return kCobaltExtensionDemuxerChannelLayout6_1;
    case AV_CH_LAYOUT_6POINT1_BACK:
      return kCobaltExtensionDemuxerChannelLayout6_1Back;
    case AV_CH_LAYOUT_6POINT1_FRONT:
      return kCobaltExtensionDemuxerChannelLayout6_1Front;
    case AV_CH_LAYOUT_7POINT0_FRONT:
      return kCobaltExtensionDemuxerChannelLayout7_0Front;
    case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
      return kCobaltExtensionDemuxerChannelLayout7_1WideBack;
    case AV_CH_LAYOUT_OCTAGONAL:
      return kCobaltExtensionDemuxerChannelLayoutOctagonal;
    default:
      return GuessChannelLayout(num_channels);
  }
}

CobaltExtensionDemuxerVideoCodecProfile ProfileIDToVideoCodecProfile(
    int profile) {
  // Clear out the CONSTRAINED & INTRA flags which are strict subsets of the
  // corresponding profiles with which they're used.
  profile &= ~FF_PROFILE_H264_CONSTRAINED;
  profile &= ~FF_PROFILE_H264_INTRA;
  switch (profile) {
    case FF_PROFILE_H264_BASELINE:
      return kCobaltExtensionDemuxerH264ProfileBaseline;
    case FF_PROFILE_H264_MAIN:
      return kCobaltExtensionDemuxerH264ProfileMain;
    case FF_PROFILE_H264_EXTENDED:
      return kCobaltExtensionDemuxerH264ProfileExtended;
    case FF_PROFILE_H264_HIGH:
      return kCobaltExtensionDemuxerH264ProfileHigh;
    case FF_PROFILE_H264_HIGH_10:
      return kCobaltExtensionDemuxerH264ProfileHigh10Profile;
    case FF_PROFILE_H264_HIGH_422:
      return kCobaltExtensionDemuxerH264ProfileHigh422Profile;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      return kCobaltExtensionDemuxerH264ProfileHigh444PredictiveProfile;
    default:
      SB_LOG(ERROR) << "Unknown profile id: " << profile;
      return kCobaltExtensionDemuxerVideoCodecProfileUnknown;
  }
}

// Attempts to parse a codec profile from |extradata|. Upon success,
// |out_profile| is populated with the profile and true is returned. Upon
// failure, false is returned and |out_profile| is not modified.
bool TryParseH264Profile(const uint8_t* extradata,
                         size_t extradata_size,
                         CobaltExtensionDemuxerVideoCodecProfile& out_profile) {
  if (extradata_size < 2) {
    return false;
  }
  const uint8_t version = extradata[0];
  if (version != 1) {
    return false;
  }
  const int profile = extradata[1];
  switch (profile) {
    case FF_PROFILE_H264_BASELINE:
      out_profile = kCobaltExtensionDemuxerH264ProfileBaseline;
      return true;
    case FF_PROFILE_H264_MAIN:
      out_profile = kCobaltExtensionDemuxerH264ProfileMain;
      return true;
    case FF_PROFILE_H264_EXTENDED:
      out_profile = kCobaltExtensionDemuxerH264ProfileExtended;
      return true;
    case FF_PROFILE_H264_HIGH:
      out_profile = kCobaltExtensionDemuxerH264ProfileHigh;
      return true;
    case FF_PROFILE_H264_HIGH_10:
      out_profile = kCobaltExtensionDemuxerH264ProfileHigh10Profile;
      return true;
    case FF_PROFILE_H264_HIGH_422:
      out_profile = kCobaltExtensionDemuxerH264ProfileHigh422Profile;
      return true;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      out_profile = kCobaltExtensionDemuxerH264ProfileHigh444PredictiveProfile;
      return true;
    default:
      SB_LOG(ERROR) << "Unknown H264 profile: " << profile;
      return false;
  }
}

// Attempts to parse a codec profile from |extradata|. Upon success,
// |out_profile| is populated with the profile and true is returned. Upon
// failure, false is returned and |out_profile| is not modified.
bool TryParseH265Profile(const uint8_t* extradata,
                         size_t extradata_size,
                         int& out_profile) {
  if (extradata_size < 2) {
    return false;
  }
  const uint8_t version = extradata[0];
  if (version != 1) {
    return false;
  }
  out_profile = extradata[1] & 0x1F;
  return true;
}

int AVIOReadOperation(void* opaque, uint8_t* buf, int buf_size) {
  auto* data_source = static_cast<CobaltExtensionDemuxerDataSource*>(opaque);
  const int bytes_read =
      data_source->BlockingRead(buf, buf_size, data_source->user_data);

  if (bytes_read == 0) {
    return AVERROR_EOF;
  } else if (bytes_read < 0) {
    return AVERROR(EIO);
  } else {
    return bytes_read;
  }
}

int64_t AVIOSeekOperation(void* opaque, int64_t offset, int whence) {
  auto* data_source = static_cast<CobaltExtensionDemuxerDataSource*>(opaque);
  switch (whence) {
    case SEEK_SET: {
      data_source->SeekTo(offset, data_source->user_data);
      break;
    }
    case SEEK_CUR: {
      const int64_t current_position =
          data_source->GetPosition(data_source->user_data);
      data_source->SeekTo(current_position + offset, data_source->user_data);
      break;
    }
    case SEEK_END: {
      const int64_t size = data_source->GetSize(data_source->user_data);
      data_source->SeekTo(size + offset, data_source->user_data);
      break;
    }
    case AVSEEK_SIZE: {
      return data_source->GetSize(data_source->user_data);
    }
    default: {
      SB_LOG(ERROR) << "Invalid whence: " << whence;
      return AVERROR(EIO);
    }
  }

  // In the case where we did a real seek, return the new position.
  return data_source->GetPosition(data_source->user_data);
}

int64_t ConvertFromTimeBaseToMicros(AVRational time_base, int64_t timestamp) {
  return FFmpegDemuxer::GetDispatch()->av_rescale_rnd(
      timestamp, time_base.num * kSbTimeSecond, time_base.den,
      static_cast<int>(AV_ROUND_NEAR_INF));
}

int64_t ConvertMicrosToTimeBase(AVRational time_base, int64_t timestamp_us) {
  return FFmpegDemuxer::GetDispatch()->av_rescale_rnd(
      timestamp_us, time_base.den, time_base.num * kSbTimeSecond,
      static_cast<int>(AV_ROUND_NEAR_INF));
}

CobaltExtensionDemuxerEncryptionScheme GetEncryptionScheme(
    const AVStream& stream) {
#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  return FFmpegDemuxer::GetDispatch()->av_dict_get(
             stream.metadata, "enc_key_id", nullptr, 0) == nullptr
             ? kCobaltExtensionDemuxerEncryptionSchemeUnencrypted
             : kCobaltExtensionDemuxerEncryptionSchemeCenc;
#else
  return kCobaltExtensionDemuxerEncryptionSchemeUnencrypted;
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
}

int64_t ExtractStartTime(AVStream* stream) {
  int64_t start_time = 0;
  if (stream->start_time != kNoFFmpegTimestamp) {
    start_time =
        ConvertFromTimeBaseToMicros(stream->time_base, stream->start_time);
  }

#if LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83
  const int32_t codec_id = stream->codecpar->codec_id;
#else
  const int32_t codec_id = stream->codec->codec_id;
#endif  // LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83

  if (stream->first_dts != kNoFFmpegTimestamp
#if FFMPEG >= 560
      && codec_id != AV_CODEC_ID_HEVC
#endif  // FFMPEG >= 560
      && codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_MPEG4) {
    const int64_t first_pts =
        ConvertFromTimeBaseToMicros(stream->time_base, stream->first_dts);
    start_time = std::min(first_pts, start_time);
  }

  return start_time;
}

// Recursively splits |s| around |delimiter| characters.
std::vector<std::string> Split(const std::string& s, char delimiter) {
  // Work from right to left, since it's faster to append to the end of a
  // vector.
  const size_t pos = s.rfind(delimiter);
  if (pos == std::string::npos) {
    // Base case.
    return {s};
  }

  // Recursive case.
  std::vector<std::string> previous_splits = Split(s.substr(0, pos), delimiter);
  previous_splits.push_back(s.substr(pos + 1));
  return previous_splits;
}

int64_t ExtractTimelineOffset(AVFormatContext* format_context) {
  const std::vector<std::string> input_formats =
      Split(format_context->iformat->name, ',');

  // The name for ff_matroska_demuxer contains "webm" in its comma-separated
  // list.
  const bool is_webm = std::any_of(
      input_formats.cbegin(), input_formats.cend(),
      +[](const std::string& format) -> bool { return format == "webm"; });

#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  if (is_webm) {
    const AVDictionaryEntry* entry = FFmpegDemuxer::GetDispatch()->av_dict_get(
        format_context->metadata, "creation_time", nullptr, 0);

    // TODO(b/231634260): properly implement this if necessary. We need to
    // return microseconds since epoch for the given date string in UTC, which
    // is harder than it sounds in pure C++.
    return 0;
  }
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  return 0;
}

const bool g_registered =
    FFMPEGDispatch::RegisterSpecialization(FFMPEG,
                                           LIBAVCODEC_VERSION_MAJOR,
                                           LIBAVFORMAT_VERSION_MAJOR,
                                           LIBAVUTIL_VERSION_MAJOR);
}  // namespace

#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
void FFmpegDemuxerImpl<FFMPEG>::ScopedPtrAVFreeContext::operator()(
    void* ptr) const {
  if (!ptr) {
    return;
  }
  auto* codec_context = static_cast<AVCodecContext*>(ptr);
  GetDispatch()->avcodec_free_context(&codec_context);
}
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

void FFmpegDemuxerImpl<FFMPEG>::ScopedPtrAVFreeAVIOContext::operator()(
    void* ptr) const {
  if (!ptr) {
    return;
  }
  // From the documentation of avio_alloc_context, AVIOContext's buffer must be
  // freed separately from the AVIOContext.
  unsigned char* buffer = static_cast<AVIOContext*>(ptr)->buffer;
  if (buffer) {
    GetDispatch()->av_free(buffer);
  }
  GetDispatch()->av_free(ptr);
}

void FFmpegDemuxerImpl<FFMPEG>::ScopedPtrAVFreePacket::operator()(
    void* ptr) const {
  if (!ptr) {
    return;
  }
  auto* packet = static_cast<AVPacket*>(ptr);
#if LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
  GetDispatch()->av_packet_free(&packet);
#else
  GetDispatch()->av_free_packet(packet);
  GetDispatch()->av_free(packet);
#endif  // LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
}

FFmpegDemuxerImpl<FFMPEG>::FFmpegDemuxerImpl(
    CobaltExtensionDemuxerDataSource* data_source)
    : data_source_(data_source) {
  SB_DCHECK(data_source_);
  SB_DCHECK(g_registered) << "Demuxer specialization registration failed.";
}

FFmpegDemuxerImpl<FFMPEG>::~FFmpegDemuxerImpl() {
  if (format_context_) {
    GetDispatch()->avformat_close_input(&format_context_);
  }
}

CobaltExtensionDemuxerStatus FFmpegDemuxerImpl<FFMPEG>::Initialize() {
  SB_DCHECK(format_context_ == nullptr);

  if (initialized_) {
    SB_LOG(ERROR)
        << "Multiple calls to FFmpegDemuxerImpl::Initialize are not allowed.";
    return kCobaltExtensionDemuxerErrorInitializationFailed;
  }
  initialized_ = true;

  avio_context_.reset(GetDispatch()->avio_alloc_context(
      static_cast<unsigned char*>(GetDispatch()->av_malloc(kAvioBufferSize)),
      kAvioBufferSize, 0,
      /*opaque=*/data_source_, &AVIOReadOperation, nullptr,
      &AVIOSeekOperation));
  avio_context_->seekable =
      data_source_->is_streaming ? 0 : AVIO_SEEKABLE_NORMAL;
  avio_context_->write_flag = 0;

  format_context_ = GetDispatch()->avformat_alloc_context();
  format_context_->flags |= AVFMT_FLAG_CUSTOM_IO;
#ifdef AVFMT_FLAG_FAST_SEEK
  format_context_->flags |= AVFMT_FLAG_FAST_SEEK;
#endif  // AVFMT_FLAG_FAST_SEEK
#ifdef AVFMT_FLAG_KEEP_SIDE_DATA
  format_context_->flags |= AVFMT_FLAG_KEEP_SIDE_DATA;
#endif  // AVFMT_FLAG_KEEP_SIDE_DATA
  format_context_->error_recognition |= AV_EF_EXPLODE;
  format_context_->pb = avio_context_.get();

  if (GetDispatch()->avformat_open_input(&format_context_, nullptr, nullptr,
                                         nullptr) < 0) {
    SB_LOG(ERROR) << "avformat_open_input failed.";
    return kCobaltExtensionDemuxerErrorCouldNotOpen;
  }
  if (GetDispatch()->avformat_find_stream_info(format_context_, nullptr) < 0) {
    SB_LOG(ERROR) << "avformat_find_stream_info failed.";
    return kCobaltExtensionDemuxerErrorCouldNotParse;
  }

  // Find the first audio stream and video stream, if present.
  // TODO(b/231632632): pick a stream based on supported codecs, not the first
  // stream present.
  for (int i = 0; i < format_context_->nb_streams; ++i) {
    AVStream* stream = format_context_->streams[i];
#if LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;
    const AVCodecID codec_id = codec_parameters->codec_id;
#else
    const AVCodecContext* codec = stream->codec;
    const AVMediaType codec_type = codec->codec_type;
    const AVCodecID codec_id = codec->codec_id;
#endif  // LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83
    // Skip streams which are not properly detected.
    if (codec_id == AV_CODEC_ID_NONE) {
      stream->discard = AVDISCARD_ALL;
      continue;
    }

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (audio_stream_) {
        continue;
      }
      audio_stream_ = stream;
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_)
        continue;
      video_stream_ = stream;
    }
  }

  if (!audio_stream_ && !video_stream_) {
    SB_LOG(ERROR) << "No audio or video stream was present.";
    return kCobaltExtensionDemuxerErrorNoSupportedStreams;
  }

  if (audio_stream_ && !ParseAudioConfig(audio_stream_, &audio_config_)) {
    return kCobaltExtensionDemuxerErrorInitializationFailed;
  }
  if (video_stream_ && !ParseVideoConfig(video_stream_, &video_config_)) {
    return kCobaltExtensionDemuxerErrorInitializationFailed;
  }

  if (format_context_->duration != kNoFFmpegTimestamp) {
    duration_us_ = ConvertFromTimeBaseToMicros(
        /*time_base=*/{1, AV_TIME_BASE}, format_context_->duration);
  }

  start_time_ = std::min(audio_stream_ ? ExtractStartTime(audio_stream_)
                                       : std::numeric_limits<int64_t>::max(),
                         video_stream_ ? ExtractStartTime(video_stream_)
                                       : std::numeric_limits<int64_t>::max());

  timeline_offset_us_ = ExtractTimelineOffset(format_context_);

  return kCobaltExtensionDemuxerOk;
}

bool FFmpegDemuxerImpl<FFMPEG>::HasAudioStream() const {
  return audio_stream_ != nullptr;
}

const CobaltExtensionDemuxerAudioDecoderConfig&
FFmpegDemuxerImpl<FFMPEG>::GetAudioConfig() const {
  return audio_config_;
}

bool FFmpegDemuxerImpl<FFMPEG>::HasVideoStream() const {
  return video_stream_ != nullptr;
}

const CobaltExtensionDemuxerVideoDecoderConfig&
FFmpegDemuxerImpl<FFMPEG>::GetVideoConfig() const {
  return video_config_;
}

SbTime FFmpegDemuxerImpl<FFMPEG>::GetDuration() const {
  return duration_us_;
}

SbTime FFmpegDemuxerImpl<FFMPEG>::GetStartTime() const {
  return start_time_;
}

SbTime FFmpegDemuxerImpl<FFMPEG>::GetTimelineOffset() const {
  return timeline_offset_us_;
}

void FFmpegDemuxerImpl<FFMPEG>::Read(CobaltExtensionDemuxerStreamType type,
                                     CobaltExtensionDemuxerReadCB read_cb,
                                     void* read_cb_user_data) {
  SB_DCHECK(type == kCobaltExtensionDemuxerStreamTypeAudio ||
            type == kCobaltExtensionDemuxerStreamTypeVideo);

  if (type == kCobaltExtensionDemuxerStreamTypeAudio) {
    SB_DCHECK(audio_stream_);
  } else {
    SB_DCHECK(video_stream_);
  }

  const AVRational time_base = type == kCobaltExtensionDemuxerStreamTypeAudio
                                   ? audio_stream_->time_base
                                   : video_stream_->time_base;

  CobaltExtensionDemuxerBuffer buffer = {};
  ScopedAVPacket packet = GetNextPacket(type);
  if (!packet) {
    // Either an error occurred or we reached EOS. Treat as EOS.
    buffer.end_of_stream = true;
    read_cb(&buffer, read_cb_user_data);
    return;
  }

  // NOTE: subtracting start_time_ is necessary because the rest of the cobalt
  // pipeline never calls the demuxer's GetStartTime() to handle the offset
  // (it assumes 0 offset).
  //
  // TODO(b/231634475): don't subtract start_time_ here if the rest of the
  // pipeline is updated to handle nonzero start times.
  buffer.pts =
      ConvertFromTimeBaseToMicros(time_base, packet->pts) - start_time_;
  buffer.duration = ConvertFromTimeBaseToMicros(time_base, packet->duration);
  buffer.is_keyframe = packet->flags & AV_PKT_FLAG_KEY;
  buffer.end_of_stream = false;
  buffer.data = packet->data;
  buffer.data_size = packet->size;

// Only newer versions support AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL.
#if LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
  std::vector<CobaltExtensionDemuxerSideData> side_data;
  for (int i = 0; i < packet->side_data_elems; ++i) {
    const AVPacketSideData& packet_side_data = packet->side_data[i];
    if (packet_side_data.type == AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL) {
      CobaltExtensionDemuxerSideData extension_side_data = {};
      extension_side_data.data = packet_side_data.data;
      extension_side_data.data_size = packet_side_data.size;
      extension_side_data.type = kCobaltExtensionDemuxerMatroskaBlockAdditional;
      side_data.push_back(std::move(extension_side_data));
    }
    // TODO(b/231635220): support other types of side data, if necessary.
  }

  if (side_data.empty()) {
    buffer.side_data = nullptr;
    buffer.side_data_elements = 0;
  } else {
    buffer.side_data = side_data.data();
    buffer.side_data_elements = side_data.size();
  }
#endif  // LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100

  read_cb(&buffer, read_cb_user_data);
}

CobaltExtensionDemuxerStatus FFmpegDemuxerImpl<FFMPEG>::Seek(
    int64_t seek_time_us) {
  // Clear any buffered packets and seek via FFmpeg.
  video_packets_.clear();
  audio_packets_.clear();

  AVStream* const stream = video_stream_ ? video_stream_ : audio_stream_;
  GetDispatch()->av_seek_frame(
      format_context_, stream->index,
      ConvertMicrosToTimeBase(stream->time_base, seek_time_us),
      AVSEEK_FLAG_BACKWARD);

  return kCobaltExtensionDemuxerOk;
}

FFmpegDemuxerImpl<FFMPEG>::ScopedAVPacket
FFmpegDemuxerImpl<FFMPEG>::CreateScopedAVPacket() {
  ScopedAVPacket packet;

#if LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
  packet.reset(GetDispatch()->av_packet_alloc());
#else
  // av_packet_alloc is not available.
  packet.reset(
      static_cast<AVPacket*>(GetDispatch()->av_malloc(sizeof(AVPacket))));
  memset(packet.get(), 0, sizeof(AVPacket));
  GetDispatch()->av_init_packet(packet.get());
#endif  // LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100

  return packet;
}

// Returns the next packet of type |type|, or nullptr if EoS has been reached
// or an error was encountered.
FFmpegDemuxerImpl<FFMPEG>::ScopedAVPacket FFmpegDemuxerImpl<
    FFMPEG>::GetNextPacket(CobaltExtensionDemuxerStreamType type) {
  // Handle the simple case: if we already have a packet buffered, just return
  // it.
  ScopedAVPacket packet = GetBufferedPacket(type);
  if (packet) {
    return packet;
  }

  // Read another packet from FFmpeg. We may have to discard a packet if it's
  // not from the right stream. Additionally, if we hit end-of-file or an
  // error, we need to return null.
  packet = CreateScopedAVPacket();
  while (true) {
    int result = GetDispatch()->av_read_frame(format_context_, packet.get());
    if (result < 0) {
      // The packet will be unref-ed when ScopedAVPacket's destructor runs.
      return nullptr;
    }

    // Determine whether to drop the packet. In that case, we need to manually
    // unref the packet, since new data will be written to it.
    if (video_stream_ && packet->stream_index == video_stream_->index) {
      if (type == kCobaltExtensionDemuxerStreamTypeVideo) {
        // We found the packet that the caller was looking for.
        return packet;
      }

      // The caller doesn't need a video packet; just buffer it and allocate a
      // new packet.
      BufferPacket(std::move(packet), kCobaltExtensionDemuxerStreamTypeVideo);
      packet = CreateScopedAVPacket();
      continue;
    } else if (audio_stream_ && packet->stream_index == audio_stream_->index) {
      if (type == kCobaltExtensionDemuxerStreamTypeAudio) {
        // We found the packet that the caller was looking for.
        return packet;
      }

      // The caller doesn't need an audio packet; just buffer it and allocate
      // a new packet.
      BufferPacket(std::move(packet), kCobaltExtensionDemuxerStreamTypeAudio);
      packet = CreateScopedAVPacket();
      continue;
    }

// This is a packet for a stream we don't care about. Unref it (clear the
// fields) and keep searching.
#if LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
    GetDispatch()->av_packet_unref(packet.get());
#else
    GetDispatch()->av_free_packet(packet.get());
#endif  // LIBAVCODEC_VERSION_INT >= LIBAVCODEC_VERSION_57_100
  }

  SB_NOTREACHED();
  return nullptr;
}

// Returns a buffered packet of type |type|, or nullptr if no buffered packet
// is available.
FFmpegDemuxerImpl<FFMPEG>::ScopedAVPacket FFmpegDemuxerImpl<
    FFMPEG>::GetBufferedPacket(CobaltExtensionDemuxerStreamType type) {
  if (type == kCobaltExtensionDemuxerStreamTypeVideo) {
    if (video_packets_.empty()) {
      return nullptr;
    }
    ScopedAVPacket packet = std::move(video_packets_.front());
    video_packets_.pop_front();
    return packet;
  } else {
    if (audio_packets_.empty()) {
      return nullptr;
    }
    ScopedAVPacket packet = std::move(audio_packets_.front());
    audio_packets_.pop_front();
    return packet;
  }
}

// Pushes |packet| into the queue specified by |type|.
void FFmpegDemuxerImpl<FFMPEG>::BufferPacket(
    ScopedAVPacket packet,
    CobaltExtensionDemuxerStreamType type) {
  if (type == kCobaltExtensionDemuxerStreamTypeVideo) {
    video_packets_.push_back(std::move(packet));
  } else {
    audio_packets_.push_back(std::move(packet));
  }
}

bool FFmpegDemuxerImpl<FFMPEG>::ParseAudioConfig(
    AVStream* audio_stream,
    CobaltExtensionDemuxerAudioDecoderConfig* config) {
  if (!config) {
    return false;
  }

  config->encryption_scheme = GetEncryptionScheme(*audio_stream);

#if LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      GetDispatch()->avcodec_alloc_context3(nullptr));
  if (!codec_context) {
    SB_LOG(ERROR) << "Could not allocate codec context.";
    return false;
  }
  if (GetDispatch()->avcodec_parameters_to_context(
          codec_context.get(), audio_stream->codecpar) < 0) {
    return false;
  }
#else
  AVCodecContext* codec_context = audio_stream->codec;
#endif  // LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83

  config->codec = AvCodecIdToAudioCodec(codec_context->codec_id);
  config->sample_format =
      AvSampleFormatToSampleFormat(codec_context->sample_fmt);
  config->channel_layout = AvChannelLayoutToChannelLayout(
      codec_context->channel_layout, codec_context->channels);
  config->samples_per_second = codec_context->sample_rate;

  // Catch a potential FFmpeg bug. See http://crbug.com/517163 for more info.
  if ((codec_context->extradata_size == 0) !=
      (codec_context->extradata == nullptr)) {
    SB_LOG(ERROR) << (codec_context->extradata == nullptr ? " NULL"
                                                          : " Non-NULL")
                  << " extra data cannot have size of "
                  << codec_context->extradata_size;
    return false;
  }

  if (codec_context->extradata_size > 0) {
    extra_audio_data_.assign(
        codec_context->extradata,
        codec_context->extradata + codec_context->extradata_size);
    config->extra_data = extra_audio_data_.data();
    config->extra_data_size = extra_audio_data_.size();
  } else {
    config->extra_data = nullptr;
    config->extra_data_size = 0;
  }

  // The spec for AC3/EAC3 audio is ETSI TS 102 366. According to sections
  // F.3.1 and F.5.1 in that spec, the sample format must be 16 bits.
  if (config->codec == kCobaltExtensionDemuxerCodecAC3 ||
      config->codec == kCobaltExtensionDemuxerCodecEAC3) {
    config->sample_format = kCobaltExtensionDemuxerSampleFormatS16;
  }

  // TODO(b/231637692): If we need to support MPEG-H, the channel layout and
  // sample format need to be set here.
  return true;
}

bool FFmpegDemuxerImpl<FFMPEG>::ParseVideoConfig(
    AVStream* video_stream,
    CobaltExtensionDemuxerVideoDecoderConfig* config) {
#if LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
      GetDispatch()->avcodec_alloc_context3(nullptr));

  if (!codec_context) {
    SB_LOG(ERROR) << "Could not allocate codec context.";
    return false;
  }
  if (GetDispatch()->avcodec_parameters_to_context(
          codec_context.get(), video_stream->codecpar) < 0) {
    return false;
  }
#else
  AVCodecContext* codec_context = video_stream->codec;
#endif  // LIBAVFORMAT_VERSION_INT >= LIBAVFORMAT_VERSION_57_83

  config->visible_rect_x = 0;
  config->visible_rect_y = 0;
  config->visible_rect_width = codec_context->width;
  config->visible_rect_height = codec_context->height;

  config->coded_width = codec_context->width;
  config->coded_height = codec_context->height;

  auto get_aspect_ratio = +[](AVRational rational) -> double {
    return rational.den == 0 ? 0.0
                             : static_cast<double>(rational.num) / rational.den;
  };

  const double aspect_ratio =
      video_stream->sample_aspect_ratio.num
          ? get_aspect_ratio(video_stream->sample_aspect_ratio)
          : codec_context->sample_aspect_ratio.num
                ? get_aspect_ratio(codec_context->sample_aspect_ratio)
                : 0.0;
  {
    double width = config->visible_rect_width;
    double height = config->visible_rect_height;
    if (aspect_ratio >= 1) {
      // Wide pixels; grow width.
      width = width * aspect_ratio;
    } else {
      // Narrow pixels; grow height.
      height = height / aspect_ratio;
    }

    width = std::round(width);
    height = std::round(height);
    if (width < 1.0 || width > std::numeric_limits<int>::max() ||
        height < 1.0 || height > std::numeric_limits<int>::max()) {
      // Invalid width and height. Just use the visible width and height.
      config->natural_width = config->visible_rect_width;
      config->natural_height = config->visible_rect_height;
    } else {
      config->natural_width = static_cast<int>(width);
      config->natural_height = static_cast<int>(height);
    }
  }
  config->codec = AvCodecIdToVideoCodec(codec_context->codec_id);

  // Without the ffmpeg decoder configured, libavformat is unable to get the
  // profile, format, or coded size. So choose sensible defaults and let
  // decoders fail later if the configuration is actually unsupported.
  config->profile = kCobaltExtensionDemuxerVideoCodecProfileUnknown;

  switch (config->codec) {
    case kCobaltExtensionDemuxerCodecH264: {
      config->profile = ProfileIDToVideoCodecProfile(codec_context->profile);
      if (config->profile == kCobaltExtensionDemuxerVideoCodecProfileUnknown &&
          codec_context->extradata && codec_context->extradata_size) {
        CobaltExtensionDemuxerVideoCodecProfile profile =
            kCobaltExtensionDemuxerVideoCodecProfileUnknown;
        // Attempt to populate profile based on extradata.
        if (TryParseH264Profile(codec_context->extradata,
                                codec_context->extradata_size, profile)) {
          config->profile = profile;
        } else {
          SB_LOG(ERROR) << "Could not parse H264 profile from extradata.";
        }
      }
      break;
    }
#ifdef FF_PROFILE_HEVC_MAIN
    case kCobaltExtensionDemuxerCodecHEVC: {
      int hevc_profile = FF_PROFILE_UNKNOWN;
      if ((codec_context->profile < FF_PROFILE_HEVC_MAIN ||
           codec_context->profile >
#ifdef FF_PROFILE_HEVC_REXT
               FF_PROFILE_HEVC_REXT
#else  // FF_PROFILE_HEVC_REXT
               FF_PROFILE_HEVC_MAIN_STILL_PICTURE
#endif  // FF_PROFILE_HEVC_REXT
           ) &&
          codec_context->extradata && codec_context->extradata_size) {
        // Attempt to populate hevc_profile based on extradata.
        if (!TryParseH265Profile(codec_context->extradata,
                                 codec_context->extradata_size, hevc_profile)) {
          SB_LOG(ERROR) << "Could not parse H265 profile from extradata.";
        }
      } else {
        hevc_profile = codec_context->profile;
      }
      switch (hevc_profile) {
        case FF_PROFILE_HEVC_MAIN:
          config->profile = kCobaltExtensionDemuxerHevcProfileMain;
          break;
        case FF_PROFILE_HEVC_MAIN_10:
          config->profile = kCobaltExtensionDemuxerHevcProfileMain10;
          break;
        case FF_PROFILE_HEVC_MAIN_STILL_PICTURE:
          config->profile = kCobaltExtensionDemuxerHevcProfileMainStillPicture;
          break;
        default:
          // Always assign a default if all heuristics fail.
          config->profile = kCobaltExtensionDemuxerHevcProfileMain;
          break;
      }
      break;
    }
#endif  // FF_PROFILE_HEVC_MAIN
    case kCobaltExtensionDemuxerCodecVP8:
      config->profile = kCobaltExtensionDemuxerVp8ProfileAny;
      break;
#ifdef FF_PROFILE_VP9_0
    case kCobaltExtensionDemuxerCodecVP9:
      switch (codec_context->profile) {
        case FF_PROFILE_VP9_0:
          config->profile = kCobaltExtensionDemuxerVp9ProfileProfile0;
          break;
        case FF_PROFILE_VP9_1:
          config->profile = kCobaltExtensionDemuxerVp9ProfileProfile1;
          break;
        case FF_PROFILE_VP9_2:
          config->profile = kCobaltExtensionDemuxerVp9ProfileProfile2;
          break;
        case FF_PROFILE_VP9_3:
          config->profile = kCobaltExtensionDemuxerVp9ProfileProfile3;
          break;
        default:
          config->profile = kCobaltExtensionDemuxerVp9ProfileMin;
          break;
      }
      break;
#endif  // FF_PROFILE_VP9_0
    case kCobaltExtensionDemuxerCodecAV1:
      config->profile = kCobaltExtensionDemuxerAv1ProfileProfileMain;
      break;
    case kCobaltExtensionDemuxerCodecTheora:
      config->profile = kCobaltExtensionDemuxerTheoraProfileAny;
      break;
    default:
      config->profile = ProfileIDToVideoCodecProfile(codec_context->profile);
  }

  config->color_space_primaries = codec_context->color_primaries;
  config->color_space_transfer = codec_context->color_trc;
  config->color_space_matrix = codec_context->colorspace;
  config->color_space_range_id =
      codec_context->color_range == AVCOL_RANGE_JPEG
          ? kCobaltExtensionDemuxerColorSpaceRangeIdFull
          : kCobaltExtensionDemuxerColorSpaceRangeIdLimited;

  // Catch a potential FFmpeg bug.
  if ((codec_context->extradata_size == 0) !=
      (codec_context->extradata == nullptr)) {
    SB_LOG(ERROR) << (codec_context->extradata == nullptr ? " NULL"
                                                          : " Non-NULL")
                  << " extra data cannot have size of "
                  << codec_context->extradata_size;
    return false;
  }

  if (codec_context->extradata_size > 0) {
    extra_video_data_.assign(
        codec_context->extradata,
        codec_context->extradata + codec_context->extradata_size);
    config->extra_data = extra_video_data_.data();
    config->extra_data_size = extra_video_data_.size();
  } else {
    config->extra_data = nullptr;
    config->extra_data_size = 0;
  }

  config->encryption_scheme = GetEncryptionScheme(*video_stream);

  return true;
}

// static
std::unique_ptr<FFmpegDemuxer> FFmpegDemuxerImpl<FFMPEG>::Create(
    CobaltExtensionDemuxerDataSource* data_source) {
  return std::unique_ptr<FFmpegDemuxerImpl<FFMPEG>>(
      new FFmpegDemuxerImpl<FFMPEG>(data_source));
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
