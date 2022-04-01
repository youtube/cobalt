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

#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

constexpr int64_t kNoFFmpegTimestamp = static_cast<int64_t>(AV_NOPTS_VALUE);
constexpr int kAvioBufferSize = 4 * 1024;  // 4KB.

// For testing, this can be overridden via TestOnlySetFFmpegDispatch below.
// Aside from that function and GetDispatch, no code should access this global
// directly. Instead, calls to the dispatch should go through GetDispatch().
FFMPEGDispatch* g_test_dispatch = nullptr;

const FFMPEGDispatch* GetDispatch() {
  if (g_test_dispatch) {
    return g_test_dispatch;
  }

  static const auto* const dispatch = FFMPEGDispatch::GetInstance();
  return dispatch;
}

struct ScopedPtrAVFreeContext {
  void operator()(void* ptr) const {
    if (!ptr) {
      return;
    }
    auto* codec_context = static_cast<AVCodecContext*>(ptr);
    GetDispatch()->avcodec_free_context(&codec_context);
  }
};

struct ScopedPtrAVFree {
  void operator()(void* ptr) const {
    if (!ptr) {
      return;
    }
    GetDispatch()->av_free(ptr);
  }
};

struct ScopedPtrAVFreePacket {
  void operator()(void* ptr) const {
    if (!ptr) {
      return;
    }
    auto* packet = static_cast<AVPacket*>(ptr);
    GetDispatch()->av_packet_free(&packet);
  }
};

using ScopedAVPacket = std::unique_ptr<AVPacket, ScopedPtrAVFreePacket>;

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
    case AV_CODEC_ID_VP9:
      return kCobaltExtensionDemuxerCodecVP9;
    case AV_CODEC_ID_HEVC:
      return kCobaltExtensionDemuxerCodecHEVC;
    case AV_CODEC_ID_AV1:
      return kCobaltExtensionDemuxerCodecAV1;
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
      std::cerr << "Unsupported channel count: " << channels << std::endl;
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
      std::cerr << "Unknown profile id: " << profile << std::endl;
      return kCobaltExtensionDemuxerVideoCodecProfileUnknown;
  }
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
      std::cerr << "Invalid whence: " << whence << std::endl;
      return AVERROR(EIO);
    }
  }

  // In the case where we did a real seek, return the new position.
  return data_source->GetPosition(data_source->user_data);
}

int64_t ConvertFromTimeBaseToMicros(AVRational time_base, int64_t timestamp) {
  return GetDispatch()->av_rescale_rnd(timestamp, time_base.num * kSbTimeSecond,
                                       time_base.den,
                                       static_cast<int>(AV_ROUND_NEAR_INF));
}

int64_t ConvertMicrosToTimeBase(AVRational time_base, int64_t timestamp_us) {
  return GetDispatch()->av_rescale_rnd(timestamp_us, time_base.den,
                                       time_base.num * kSbTimeSecond,
                                       static_cast<int>(AV_ROUND_NEAR_INF));
}

CobaltExtensionDemuxerEncryptionScheme GetEncryptionScheme(
    const AVStream& stream) {
  return GetDispatch()->av_dict_get(stream.metadata, "enc_key_id", nullptr,
                                    0) == nullptr
             ? kCobaltExtensionDemuxerEncryptionSchemeUnencrypted
             : kCobaltExtensionDemuxerEncryptionSchemeCenc;
}

int64_t ExtractStartTime(AVStream* stream) {
  int64_t start_time = 0;
  if (stream->start_time != kNoFFmpegTimestamp) {
    start_time =
        ConvertFromTimeBaseToMicros(stream->time_base, stream->start_time);
  }

  if (stream->first_dts != kNoFFmpegTimestamp &&
      stream->codecpar->codec_id != AV_CODEC_ID_HEVC &&
      stream->codecpar->codec_id != AV_CODEC_ID_H264 &&
      stream->codecpar->codec_id != AV_CODEC_ID_MPEG4) {
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

  if (is_webm) {
    const AVDictionaryEntry* entry = GetDispatch()->av_dict_get(
        format_context->metadata, "creation_time", nullptr, 0);

    // TODO(b/231634260): properly implement this if necessary. We need to
    // return microseconds since epoch for the given date string in UTC, which
    // is harder than it sounds in pure C++.
    return 0;
  }
  return 0;
}

// A demuxer implemented via FFmpeg. Calls to FFmpeg go through GetDispatch()
// (defined above).
// The API of this class mirrors that of CobaltExtensionDemuxer; those calls get
// forwarded to an instance of this class.
class FFmpegDemuxer {
 public:
  explicit FFmpegDemuxer(CobaltExtensionDemuxerDataSource* data_source)
      : data_source_(data_source) {
    assert(data_source_);
  }

  // Disallow copy and assign.
  FFmpegDemuxer(const FFmpegDemuxer&) = delete;
  FFmpegDemuxer& operator=(const FFmpegDemuxer&) = delete;

  ~FFmpegDemuxer() {
    if (format_context_) {
      GetDispatch()->avformat_close_input(&format_context_);
    }
  }

  CobaltExtensionDemuxerStatus Initialize() {
    assert(format_context_ == nullptr);

    if (initialized_) {
      std::cerr
          << "Multiple calls to FFmpegDemuxer::Initialize are not allowed."
          << std::endl;
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
    format_context_->flags |= AVFMT_FLAG_FAST_SEEK;
    format_context_->flags |= AVFMT_FLAG_KEEP_SIDE_DATA;
    format_context_->error_recognition |= AV_EF_EXPLODE;
    format_context_->pb = avio_context_.get();

    if (GetDispatch()->avformat_open_input(&format_context_, nullptr, nullptr,
                                           nullptr) < 0) {
      std::cerr << "avformat_open_input failed." << std::endl;
      return kCobaltExtensionDemuxerErrorCouldNotOpen;
    }
    if (GetDispatch()->avformat_find_stream_info(format_context_, nullptr) <
        0) {
      std::cerr << "avformat_find_stream_info failed." << std::endl;
      return kCobaltExtensionDemuxerErrorCouldNotParse;
    }

    // Find the first audio stream and video stream, if present.
    // TODO(b/231632632): pick a stream based on supported codecs, not the first
    // stream present.
    for (int i = 0; i < format_context_->nb_streams; ++i) {
      AVStream* stream = format_context_->streams[i];
      const AVCodecParameters* codec_parameters = stream->codecpar;
      const AVMediaType codec_type = codec_parameters->codec_type;
      const AVCodecID codec_id = codec_parameters->codec_id;
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
      std::cerr << "No audio or video stream was present." << std::endl;
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

  bool HasAudioStream() const { return audio_stream_ != nullptr; }

  const CobaltExtensionDemuxerAudioDecoderConfig& GetAudioConfig() const {
    return audio_config_;
  }

  bool HasVideoStream() const { return video_stream_ != nullptr; }

  const CobaltExtensionDemuxerVideoDecoderConfig& GetVideoConfig() const {
    return video_config_;
  }

  SbTime GetDuration() const { return duration_us_; }

  SbTime GetStartTime() const { return start_time_; }

  SbTime GetTimelineOffset() const { return timeline_offset_us_; }

  void Read(CobaltExtensionDemuxerStreamType type,
            CobaltExtensionDemuxerReadCB read_cb,
            void* read_cb_user_data) {
    assert(type == kCobaltExtensionDemuxerStreamTypeAudio ||
           type == kCobaltExtensionDemuxerStreamTypeVideo);

    if (type == kCobaltExtensionDemuxerStreamTypeAudio) {
      assert(audio_stream_);
    } else {
      assert(video_stream_);
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

    std::vector<CobaltExtensionDemuxerSideData> side_data;
    for (int i = 0; i < packet->side_data_elems; ++i) {
      const AVPacketSideData& packet_side_data = packet->side_data[i];
      if (packet_side_data.type == AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL) {
        CobaltExtensionDemuxerSideData extension_side_data = {};
        extension_side_data.data = packet_side_data.data;
        extension_side_data.data_size = packet_side_data.size;
        extension_side_data.type =
            kCobaltExtensionDemuxerMatroskaBlockAdditional;
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

    read_cb(&buffer, read_cb_user_data);
  }

  CobaltExtensionDemuxerStatus Seek(int64_t seek_time_us) {
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

 private:
  // Returns the next packet of type |type|, or nullptr if EoS has been reached
  // or an error was encountered.
  ScopedAVPacket GetNextPacket(CobaltExtensionDemuxerStreamType type) {
    // Handle the simple case: if we already have a packet buffered, just return
    // it.
    ScopedAVPacket packet = GetBufferedPacket(type);
    if (packet)
      return packet;

    // Read another packet from FFmpeg. We may have to discard a packet if it's
    // not from the right stream. Additionally, if we hit end-of-file or an
    // error, we need to return null.
    packet.reset(GetDispatch()->av_packet_alloc());
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
        packet.reset(GetDispatch()->av_packet_alloc());
        continue;
      } else if (audio_stream_ &&
                 packet->stream_index == audio_stream_->index) {
        if (type == kCobaltExtensionDemuxerStreamTypeAudio) {
          // We found the packet that the caller was looking for.
          return packet;
        }

        // The caller doesn't need an audio packet; just buffer it and allocate
        // a new packet.
        BufferPacket(std::move(packet), kCobaltExtensionDemuxerStreamTypeAudio);
        packet.reset(GetDispatch()->av_packet_alloc());
        continue;
      }

      // This is a packet for a stream we don't care about. Unref it and keep
      // searching.
      GetDispatch()->av_packet_unref(packet.get());
    }

    // We should never reach this point.
    assert(false);
    return nullptr;
  }

  // Returns a buffered packet of type |type|, or nullptr if no buffered packet
  // is available.
  ScopedAVPacket GetBufferedPacket(CobaltExtensionDemuxerStreamType type) {
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
  void BufferPacket(ScopedAVPacket packet,
                    CobaltExtensionDemuxerStreamType type) {
    if (type == kCobaltExtensionDemuxerStreamTypeVideo) {
      video_packets_.push_back(std::move(packet));
    } else {
      audio_packets_.push_back(std::move(packet));
    }
  }

  bool ParseAudioConfig(AVStream* audio_stream,
                        CobaltExtensionDemuxerAudioDecoderConfig* config) {
    if (!config) {
      return false;
    }

    config->encryption_scheme = GetEncryptionScheme(*audio_stream);

    std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
        GetDispatch()->avcodec_alloc_context3(nullptr));
    if (!codec_context) {
      std::cerr << "Could not allocate codec context." << std::endl;
      return false;
    }
    if (GetDispatch()->avcodec_parameters_to_context(
            codec_context.get(), audio_stream->codecpar) < 0) {
      return false;
    }

    config->codec = AvCodecIdToAudioCodec(codec_context->codec_id);
    config->sample_format =
        AvSampleFormatToSampleFormat(codec_context->sample_fmt);
    config->channel_layout = AvChannelLayoutToChannelLayout(
        codec_context->channel_layout, codec_context->channels);
    config->samples_per_second = codec_context->sample_rate;

    // Catch a potential FFmpeg bug. See http://crbug.com/517163 for more info.
    if ((codec_context->extradata_size == 0) !=
        (codec_context->extradata == nullptr)) {
      std::cerr << (codec_context->extradata == nullptr ? " NULL" : " Non-NULL")
                << " extra data cannot have size of "
                << codec_context->extradata_size << "." << std::endl;
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

  bool ParseVideoConfig(AVStream* video_stream,
                        CobaltExtensionDemuxerVideoDecoderConfig* config) {
    std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context(
        GetDispatch()->avcodec_alloc_context3(nullptr));
    if (!codec_context) {
      std::cerr << "Could not allocate codec context." << std::endl;
      return false;
    }
    if (GetDispatch()->avcodec_parameters_to_context(
            codec_context.get(), video_stream->codecpar) < 0) {
      return false;
    }

    config->visible_rect_x = 0;
    config->visible_rect_y = 0;
    config->visible_rect_width = codec_context->width;
    config->visible_rect_height = codec_context->height;

    config->coded_width = codec_context->width;
    config->coded_height = codec_context->height;

    auto get_aspect_ratio = +[](AVRational rational) -> double {
      return rational.den == 0
                 ? 0.0
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
        if (config->profile ==
                kCobaltExtensionDemuxerVideoCodecProfileUnknown &&
            codec_context->extradata && codec_context->extradata_size) {
          // TODO(b/231631898): handle the extra data here, if necessary.
          std::cerr << "Extra data is not currently handled." << std::endl;
        }
        break;
      }
      case kCobaltExtensionDemuxerCodecHEVC: {
        int hevc_profile = FF_PROFILE_UNKNOWN;
        if ((codec_context->profile < FF_PROFILE_HEVC_MAIN ||
             codec_context->profile > FF_PROFILE_HEVC_REXT) &&
            codec_context->extradata && codec_context->extradata_size) {
          // TODO(b/231631898): handle the extra data here, if necessary.
          std::cerr << "Extra data is not currently handled." << std::endl;
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
            config->profile =
                kCobaltExtensionDemuxerHevcProfileMainStillPicture;
            break;
          default:
            // Always assign a default if all heuristics fail.
            config->profile = kCobaltExtensionDemuxerHevcProfileMain;
            break;
        }
        break;
      }
      case kCobaltExtensionDemuxerCodecVP8:
        config->profile = kCobaltExtensionDemuxerVp8ProfileAny;
        break;
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
      std::cerr << (codec_context->extradata == nullptr ? " NULL" : " Non-NULL")
                << " extra data cannot have size of "
                << codec_context->extradata_size << "." << std::endl;
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

  CobaltExtensionDemuxerDataSource* data_source_ = nullptr;
  AVStream* video_stream_ = nullptr;
  AVStream* audio_stream_ = nullptr;
  std::deque<ScopedAVPacket> video_packets_;
  std::deque<ScopedAVPacket> audio_packets_;

  std::vector<uint8_t> extra_audio_data_;
  std::vector<uint8_t> extra_video_data_;

  // These will only be properly populated if the corresponding AVStream is not
  // null.
  CobaltExtensionDemuxerAudioDecoderConfig audio_config_ = {};
  CobaltExtensionDemuxerVideoDecoderConfig video_config_ = {};

  bool initialized_ = false;
  int64_t start_time_ = 0L;
  int64_t duration_us_ = 0L;
  int64_t timeline_offset_us_ = 0L;

  // FFmpeg-related structs.
  std::unique_ptr<AVIOContext, ScopedPtrAVFree> avio_context_;
  AVFormatContext* format_context_ = nullptr;
};

CobaltExtensionDemuxerStatus FFmpegDemuxer_Initialize(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->Initialize();
}

CobaltExtensionDemuxerStatus FFmpegDemuxer_Seek(int64_t seek_time_us,
                                                void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->Seek(seek_time_us);
}

SbTime FFmpegDemuxer_GetStartTime(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetStartTime();
}

SbTime FFmpegDemuxer_GetTimelineOffset(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetTimelineOffset();
}

void FFmpegDemuxer_Read(CobaltExtensionDemuxerStreamType type,
                        CobaltExtensionDemuxerReadCB read_cb,
                        void* read_cb_user_data,
                        void* user_data) {
  static_cast<FFmpegDemuxer*>(user_data)->Read(type, read_cb,
                                               read_cb_user_data);
}

bool FFmpegDemuxer_GetAudioConfig(
    CobaltExtensionDemuxerAudioDecoderConfig* config,
    void* user_data) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(user_data);
  if (!ffmpeg_demuxer->HasAudioStream()) {
    return false;
  }
  *config = ffmpeg_demuxer->GetAudioConfig();
  return true;
}

bool FFmpegDemuxer_GetVideoConfig(
    CobaltExtensionDemuxerVideoDecoderConfig* config,
    void* user_data) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(user_data);
  if (!ffmpeg_demuxer->HasVideoStream()) {
    return false;
  }
  *config = ffmpeg_demuxer->GetVideoConfig();
  return true;
}

SbTime FFmpegDemuxer_GetDuration(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetDuration();
}

CobaltExtensionDemuxer* CreateFFmpegDemuxer(
    CobaltExtensionDemuxerDataSource* data_source,
    CobaltExtensionDemuxerAudioCodec* supported_audio_codecs,
    int64_t supported_audio_codecs_size,
    CobaltExtensionDemuxerVideoCodec* supported_video_codecs,
    int64_t supported_video_codecs_size) {
  // TODO(b/231632632): utilize supported_audio_codecs and
  // supported_video_codecs. They should ultimately be passed to FFmpegDemuxer's
  // ctor (as vectors), and the demuxer should fail fast for unsupported codecs.
  return new CobaltExtensionDemuxer{
      &FFmpegDemuxer_Initialize,     &FFmpegDemuxer_Seek,
      &FFmpegDemuxer_GetStartTime,   &FFmpegDemuxer_GetTimelineOffset,
      &FFmpegDemuxer_Read,           &FFmpegDemuxer_GetAudioConfig,
      &FFmpegDemuxer_GetVideoConfig, &FFmpegDemuxer_GetDuration,
      new FFmpegDemuxer(data_source)};
}

void DestroyFFmpegDemuxer(CobaltExtensionDemuxer* demuxer) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(demuxer->user_data);
  delete ffmpeg_demuxer;
  delete demuxer;
}

const CobaltExtensionDemuxerApi kDemuxerApi = {
    /*name=*/kCobaltExtensionDemuxerApi,
    /*version=*/1,
    /*CreateDemuxer=*/&CreateFFmpegDemuxer,
    /*DestroyDemuxer=*/&DestroyFFmpegDemuxer};

}  // namespace

const CobaltExtensionDemuxerApi* GetFFmpegDemuxerApi() {
  return &kDemuxerApi;
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
void TestOnlySetFFmpegDispatch(FFMPEGDispatch* dispatch) {
  g_test_dispatch = dispatch;
}
#endif

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
