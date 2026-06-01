// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_util.h"

#include <algorithm>
#include <cctype>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {

AudioStreamInfo& AudioStreamInfo::operator=(
    const SbMediaAudioStreamInfo& that) {
  if (that.audio_specific_config_size > 0) {
    SB_DCHECK(that.audio_specific_config);
  }

  codec_ = that.codec;

  if (codec_ == kSbMediaAudioCodecNone) {
    mime_.clear();
    audio_specific_config_.clear();
    return *this;
  }

  SB_DCHECK(that.mime);

  mime_ = that.mime;
  number_of_channels_ = that.number_of_channels;
  samples_per_second_ = that.samples_per_second;
  bits_per_sample_ = that.bits_per_sample;

  if (that.audio_specific_config_size > 0) {
    auto config = static_cast<const uint8_t*>(that.audio_specific_config);
    audio_specific_config_.assign(config,
                                  config + that.audio_specific_config_size);
  } else {
    audio_specific_config_.clear();
  }
  return *this;
}

void AudioStreamInfo::ConvertTo(
    SbMediaAudioStreamInfo* audio_stream_info) const {
  SB_DCHECK(audio_stream_info);

  *audio_stream_info = {};

  audio_stream_info->codec = codec_;
  audio_stream_info->mime = mime_.c_str();
  audio_stream_info->number_of_channels = number_of_channels_;
  audio_stream_info->samples_per_second = samples_per_second_;
  audio_stream_info->bits_per_sample = bits_per_sample_;
  if (!audio_specific_config_.empty()) {
    audio_stream_info->audio_specific_config = audio_specific_config_.data();
    audio_stream_info->audio_specific_config_size =
        static_cast<uint16_t>(audio_specific_config_.size());
  }
}

bool operator==(const AudioStreamInfo& left, const AudioStreamInfo& right) {
  if (left.codec() == kSbMediaAudioCodecNone &&
      right.codec() == kSbMediaAudioCodecNone) {
    return true;
  }

  return left.codec() == right.codec() && left.mime() == right.mime() &&
         left.number_of_channels() == right.number_of_channels() &&
         left.samples_per_second() == right.samples_per_second() &&
         left.bits_per_sample() == right.bits_per_sample() &&
         left.audio_specific_config() == right.audio_specific_config();
}

bool operator!=(const AudioStreamInfo& left, const AudioStreamInfo& right) {
  return !(left == right);
}

std::ostream& operator<<(std::ostream& os, const AudioStreamInfo& info) {
  return os << "{codec=" << GetMediaAudioCodecName(info.codec())
            << ", mime=" << (info.mime().empty() ? "(empty)" : info.mime())
            << ", channels=" << info.number_of_channels()
            << ", samples_per_second="
            << FormatWithDigitSeparators(info.samples_per_second())
            << ", bits_per_sample=" << info.bits_per_sample() << "}";
}

AudioSampleInfo& AudioSampleInfo::operator=(
    const SbMediaAudioSampleInfo& that) {
  stream_info_ = that.stream_info;
  discarded_duration_from_front_ = that.discarded_duration_from_front;
  discarded_duration_from_back_ = that.discarded_duration_from_back;

  return *this;
}

void AudioSampleInfo::ConvertTo(
    SbMediaAudioSampleInfo* audio_sample_info) const {
  SB_DCHECK(audio_sample_info);

  *audio_sample_info = {};
  stream_info_.ConvertTo(&audio_sample_info->stream_info);
  audio_sample_info->discarded_duration_from_front =
      discarded_duration_from_front_;
  audio_sample_info->discarded_duration_from_back =
      discarded_duration_from_back_;
}

VideoStreamInfo& VideoStreamInfo::operator=(
    const SbMediaVideoStreamInfo& that) {
  codec_ = that.codec;

  if (codec_ == kSbMediaVideoCodecNone) {
    mime_.clear();
    max_video_capabilities_.clear();
    return *this;
  }

  SB_DCHECK(that.mime);
  SB_DCHECK(that.max_video_capabilities);

  mime_ = that.mime;
  max_video_capabilities_ = that.max_video_capabilities;
  frame_size_ = {that.frame_width, that.frame_height};
  color_metadata_ = that.color_metadata;
  return *this;
}

void VideoStreamInfo::ConvertTo(
    SbMediaVideoStreamInfo* video_stream_info) const {
  SB_DCHECK(video_stream_info);

  *video_stream_info = {};

  video_stream_info->codec = codec_;
  video_stream_info->mime = mime_.c_str();
  video_stream_info->max_video_capabilities = max_video_capabilities_.c_str();
  video_stream_info->frame_width = frame_size_.width;
  video_stream_info->frame_height = frame_size_.height;
  video_stream_info->color_metadata = color_metadata_;
}

bool operator==(const VideoStreamInfo& left, const VideoStreamInfo& right) {
  if (left.codec() == kSbMediaVideoCodecNone &&
      right.codec() == kSbMediaVideoCodecNone) {
    return true;
  }

  return left.codec() == right.codec() && left.mime() == right.mime() &&
         left.max_video_capabilities() == right.max_video_capabilities() &&
         left.frame_size() == right.frame_size() &&
         left.color_metadata() == right.color_metadata();
}

bool operator!=(const VideoStreamInfo& left, const VideoStreamInfo& right) {
  return !(left == right);
}

VideoSampleInfo& VideoSampleInfo::operator=(
    const SbMediaVideoSampleInfo& that) {
  stream_info_ = that.stream_info;
  is_key_frame_ = that.is_key_frame;
  return *this;
}

void VideoSampleInfo::ConvertTo(
    SbMediaVideoSampleInfo* video_sample_info) const {
  SB_DCHECK(video_sample_info);

  *video_sample_info = {};
  stream_info_.ConvertTo(&video_sample_info->stream_info);
  video_sample_info->is_key_frame = is_key_frame_;
}

std::ostream& operator<<(std::ostream& os, const VideoSampleInfo& sample_info) {
  const auto& stream_info = sample_info.stream_info();

  if (stream_info.codec() == kSbMediaVideoCodecNone) {
    return os << "codec: " << GetMediaVideoCodecName(stream_info.codec());
  }

  os << "codec: " << GetMediaVideoCodecName(stream_info.codec()) << ", ";
  os << "mime: " << stream_info.mime()
     << ", max video capabilities: " << stream_info.max_video_capabilities()
     << ", ";

  if (sample_info.is_key_frame()) {
    os << "key frame, ";
  }

  os << stream_info.frame_size() << ' ';
  os << '(' << stream_info.color_metadata() << ')';

  return os;
}

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id) {
  SB_DCHECK(bit_depth == 8 || bit_depth == 10);

  if (bit_depth != 8) {
    return false;
  }

  if (primary_id != kSbMediaPrimaryIdBt709 &&
      primary_id != kSbMediaPrimaryIdUnspecified &&
      primary_id != kSbMediaPrimaryIdSmpte170M) {
    return false;
  }

  if (transfer_id != kSbMediaTransferIdBt709 &&
      transfer_id != kSbMediaTransferIdUnspecified &&
      transfer_id != kSbMediaTransferIdSmpte170M) {
    return false;
  }

  if (matrix_id != kSbMediaMatrixIdBt709 &&
      matrix_id != kSbMediaMatrixIdUnspecified &&
      matrix_id != kSbMediaMatrixIdSmpte170M) {
    return false;
  }

  return true;
}

bool IsSDRVideo(const char* mime) {
  SB_DCHECK(mime);

  if (!mime) {
    SB_LOG(WARNING) << mime << " is empty, assuming sdr video.";
    return true;
  }

  auto mime_type = MimeType::Create(mime);
  if (!mime_type) {
    SB_LOG(WARNING) << mime << " is not a valid mime type, assuming sdr video.";
    return true;
  }
  const std::vector<std::string> codecs = mime_type->GetCodecs();
  if (codecs.empty()) {
    SB_LOG(WARNING) << mime << " contains no codecs, assuming sdr video.";
    return true;
  }
  if (codecs.size() > 1) {
    SB_LOG(WARNING) << mime
                    << " contains more than one codecs, assuming sdr video.";
    return true;
  }

  SbMediaVideoCodec video_codec;
  int profile = -1;
  int level = -1;
  int bit_depth = 8;
  SbMediaPrimaryId primary_id = kSbMediaPrimaryIdUnspecified;
  SbMediaTransferId transfer_id = kSbMediaTransferIdUnspecified;
  SbMediaMatrixId matrix_id = kSbMediaMatrixIdUnspecified;

  if (!ParseVideoCodec(codecs[0].c_str(), &video_codec, &profile, &level,
                       &bit_depth, &primary_id, &transfer_id, &matrix_id)) {
    SB_LOG(WARNING) << "ParseVideoCodec() failed on mime: " << mime
                    << ", assuming sdr video.";
    return true;
  }

  SB_DCHECK_NE(video_codec, kSbMediaVideoCodecNone);
  // TODO: Consider to consolidate the two IsSDRVideo() implementations by
  //       calling IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id).
  return bit_depth == 8;
}

int GetBytesPerSample(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;
    case kSbMediaAudioSampleTypeFloat32:
      return 4;
  }

  SB_NOTREACHED();
  return 4;
}

std::string GetStringRepresentation(const uint8_t* data, const int size) {
  std::string result;

  for (int i = 0; i < size; ++i) {
    if (std::isspace(data[i])) {
      result += ' ';
    } else if (std::isprint(data[i])) {
      result += data[i];
    } else {
      result += '?';
    }
  }

  return result;
}

std::string GetMixedRepresentation(const uint8_t* data,
                                   const int size,
                                   const int bytes_per_line) {
  std::string result;

  for (int i = 0; i < size; i += bytes_per_line) {
    if (i + bytes_per_line <= size) {
      result += HexEncode(data + i, bytes_per_line);
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_per_line);
      result += '\n';
    } else {
      int bytes_left = size - i;
      result += HexEncode(data + i, bytes_left);
      result += std::string((bytes_per_line - bytes_left) * 2, ' ');
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_left);
      result += std::string(bytes_per_line - bytes_left, ' ');
      result += '\n';
    }
  }

  return result;
}

bool IsAudioSampleInfoSubstantiallyDifferent(const AudioStreamInfo& left,
                                             const AudioStreamInfo& right) {
  return left.codec() != right.codec() ||
         left.samples_per_second() != right.samples_per_second() ||
         left.number_of_channels() != right.number_of_channels() ||
         left.audio_specific_config() != right.audio_specific_config();
}

int AudioDurationToFrames(int64_t duration, int samples_per_second) {
  SB_DCHECK_GT(samples_per_second, 0)
      << "samples_per_second has to be greater than 0";
  // The same as `frames = (duration / 1'000'000) * samples_per_second`,
  // switch order to avoid precision loss due to integer division.
  return duration * samples_per_second / 1'000'000LL;
}

int64_t AudioFramesToDuration(int frames, int samples_per_second) {
  SB_DCHECK_GT(samples_per_second, 0)
      << "samples_per_second has to be greater than 0";
  return frames * 1'000'000LL / std::max(samples_per_second, 1);
}

int AlignUp(int value, int alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace starboard
