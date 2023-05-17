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

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

const int64_t kDefaultBitRate = 0;
const int64_t kDefaultAudioChannels = 2;

template <typename StreamInfo>
void Assign(const StreamInfo& source, AudioStreamInfo* dest) {
  SB_DCHECK(dest);

  if (source.audio_specific_config_size > 0) {
    SB_DCHECK(source.audio_specific_config);
  }

  dest->codec = source.codec;

  if (dest->codec == kSbMediaAudioCodecNone) {
    dest->mime.clear();
    dest->audio_specific_config.clear();
    return;
  }

  SB_DCHECK(source.mime);

  dest->mime = source.mime;
  dest->number_of_channels = source.number_of_channels;
  dest->samples_per_second = source.samples_per_second;
  dest->bits_per_sample = source.bits_per_sample;

  auto config = static_cast<const uint8_t*>(source.audio_specific_config);
  dest->audio_specific_config.assign(
      config, config + source.audio_specific_config_size);
}

template <typename StreamInfo>
void Assign(const StreamInfo& source, VideoStreamInfo* dest) {
  SB_DCHECK(dest);

  dest->codec = source.codec;

  if (dest->codec == kSbMediaVideoCodecNone) {
    dest->mime.clear();
    dest->max_video_capabilities.clear();
    return;
  }

  SB_DCHECK(source.mime);
  SB_DCHECK(source.max_video_capabilities);

  dest->mime = source.mime;
  dest->max_video_capabilities = source.max_video_capabilities;
  dest->frame_width = source.frame_width;
  dest->frame_height = source.frame_height;
  dest->color_metadata = source.color_metadata;
}

template <typename StreamInfo>
void Assign(const AudioStreamInfo& source, StreamInfo* dest) {
  SB_DCHECK(dest);

  *dest = {};

  dest->codec = source.codec;
  dest->mime = source.mime.c_str();
  dest->number_of_channels = source.number_of_channels;
  dest->samples_per_second = source.samples_per_second;
  dest->bits_per_sample = source.bits_per_sample;
  if (!source.audio_specific_config.empty()) {
    dest->audio_specific_config = source.audio_specific_config.data();
    dest->audio_specific_config_size =
        static_cast<uint16_t>(source.audio_specific_config.size());
  }
}

template <typename StreamInfo>
void Assign(const VideoStreamInfo& source, StreamInfo* dest) {
  SB_DCHECK(dest);

  *dest = {};

  dest->codec = source.codec;
  dest->mime = source.mime.c_str();
  dest->max_video_capabilities = source.max_video_capabilities.c_str();
  dest->frame_width = source.frame_width;
  dest->frame_height = source.frame_height;
  dest->color_metadata = source.color_metadata;
}

}  // namespace

AudioStreamInfo& AudioStreamInfo::operator=(
    const SbMediaAudioStreamInfo& that) {
  Assign(that, this);
  return *this;
}

AudioStreamInfo& AudioStreamInfo::operator=(
    const CobaltExtensionEnhancedAudioMediaAudioStreamInfo& that) {
  Assign(that, this);
  return *this;
}

void AudioStreamInfo::ConvertTo(
    SbMediaAudioStreamInfo* audio_stream_info) const {
  Assign(*this, audio_stream_info);

#if SB_API_VERSION < 15
  SB_DCHECK(audio_stream_info);
  audio_stream_info->format_tag = 0xff;
  audio_stream_info->block_alignment = 4;
  audio_stream_info->average_bytes_per_second =
      audio_stream_info->samples_per_second *
      audio_stream_info->number_of_channels *
      audio_stream_info->bits_per_sample / 8;
#endif  // SB_API_VERSION < 15
}

void AudioStreamInfo::ConvertTo(
    CobaltExtensionEnhancedAudioMediaAudioStreamInfo* audio_stream_info) const {
  Assign(*this, audio_stream_info);
}

bool operator==(const AudioStreamInfo& left, const AudioStreamInfo& right) {
  if (left.codec == kSbMediaAudioCodecNone &&
      right.codec == kSbMediaAudioCodecNone) {
    return true;
  }

  return left.codec == right.codec && left.mime == right.mime &&
         left.number_of_channels == right.number_of_channels &&
         left.samples_per_second == right.samples_per_second &&
         left.bits_per_sample == right.bits_per_sample &&
         left.audio_specific_config == right.audio_specific_config;
}

bool operator!=(const AudioStreamInfo& left, const AudioStreamInfo& right) {
  return !(left == right);
}

AudioSampleInfo& AudioSampleInfo::operator=(
    const SbMediaAudioSampleInfo& that) {
#if SB_API_VERSION >= 15
  stream_info = that.stream_info;
  discarded_duration_from_front = that.discarded_duration_from_front;
  discarded_duration_from_back = that.discarded_duration_from_back;
#else   // SB_API_VERSION >= 15
  stream_info = that;
#endif  // SB_API_VERSION >= 15

  return *this;
}

AudioSampleInfo& AudioSampleInfo::operator=(
    const CobaltExtensionEnhancedAudioMediaAudioSampleInfo& that) {
  stream_info = that.stream_info;
  discarded_duration_from_front = that.discarded_duration_from_front;
  discarded_duration_from_back = that.discarded_duration_from_back;
  return *this;
}

void AudioSampleInfo::ConvertTo(
    SbMediaAudioSampleInfo* audio_sample_info) const {
  SB_DCHECK(audio_sample_info);

  *audio_sample_info = {};
#if SB_API_VERSION >= 15
  stream_info.ConvertTo(&audio_sample_info->stream_info);
  audio_sample_info->discarded_duration_from_front =
      discarded_duration_from_front;
  audio_sample_info->discarded_duration_from_back =
      discarded_duration_from_back;
#else   // SB_API_VERSION >= 15
  stream_info.ConvertTo(audio_sample_info);
#endif  // SB_API_VERSION >= 15
}

void AudioSampleInfo::ConvertTo(
    CobaltExtensionEnhancedAudioMediaAudioSampleInfo* audio_sample_info) const {
  SB_DCHECK(audio_sample_info);

  *audio_sample_info = {};
  stream_info.ConvertTo(&audio_sample_info->stream_info);
  audio_sample_info->discarded_duration_from_front =
      discarded_duration_from_front;
  audio_sample_info->discarded_duration_from_back =
      discarded_duration_from_back;
}

VideoStreamInfo& VideoStreamInfo::operator=(
    const SbMediaVideoStreamInfo& that) {
  Assign(that, this);
  return *this;
}

VideoStreamInfo& VideoStreamInfo::operator=(
    const CobaltExtensionEnhancedAudioMediaVideoStreamInfo& that) {
  Assign(that, this);
  return *this;
}

void VideoStreamInfo::ConvertTo(
    SbMediaVideoStreamInfo* video_stream_info) const {
  Assign(*this, video_stream_info);
}

void VideoStreamInfo::ConvertTo(
    CobaltExtensionEnhancedAudioMediaVideoStreamInfo* video_stream_info) const {
  Assign(*this, video_stream_info);
}

bool operator==(const VideoStreamInfo& left, const VideoStreamInfo& right) {
  if (left.codec == kSbMediaVideoCodecNone &&
      right.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  return left.codec == right.codec && left.mime == right.mime &&
         left.max_video_capabilities == right.max_video_capabilities &&
         left.frame_width == right.frame_width &&
         left.frame_height == right.frame_height &&
         left.color_metadata == right.color_metadata;
}

bool operator!=(const VideoStreamInfo& left, const VideoStreamInfo& right) {
  return !(left == right);
}

VideoSampleInfo& VideoSampleInfo::operator=(
    const SbMediaVideoSampleInfo& that) {
#if SB_API_VERSION >= 15
  stream_info = that.stream_info;
#else   // SB_API_VERSION >= 15
  stream_info = that;
#endif  // SB_API_VERSION >= 15
  is_key_frame = that.is_key_frame;
  return *this;
}

VideoSampleInfo& VideoSampleInfo::operator=(
    const CobaltExtensionEnhancedAudioMediaVideoSampleInfo& that) {
  stream_info = that.stream_info;
  is_key_frame = that.is_key_frame;
  return *this;
}

void VideoSampleInfo::ConvertTo(
    SbMediaVideoSampleInfo* video_sample_info) const {
  SB_DCHECK(video_sample_info);

  *video_sample_info = {};
#if SB_API_VERSION >= 15
  stream_info.ConvertTo(&video_sample_info->stream_info);
#else   // SB_API_VERSION >= 15
  stream_info.ConvertTo(video_sample_info);
#endif  // SB_API_VERSION >= 15
  video_sample_info->is_key_frame = is_key_frame;
}

void VideoSampleInfo::ConvertTo(
    CobaltExtensionEnhancedAudioMediaVideoSampleInfo* video_sample_info) const {
  SB_DCHECK(video_sample_info);

  *video_sample_info = {};
  stream_info.ConvertTo(&video_sample_info->stream_info);
  video_sample_info->is_key_frame = is_key_frame;
}

std::ostream& operator<<(std::ostream& os, const VideoSampleInfo& sample_info) {
  const auto& stream_info = sample_info.stream_info;

  if (stream_info.codec == kSbMediaVideoCodecNone) {
    return os << "codec: " << GetMediaVideoCodecName(stream_info.codec);
  }

  os << "codec: " << GetMediaVideoCodecName(stream_info.codec) << ", ";
  os << "mime: " << stream_info.mime
     << ", max video capabilities: " << stream_info.max_video_capabilities
     << ", ";

  if (sample_info.is_key_frame) {
    os << "key frame, ";
  }

  os << stream_info.frame_width << 'x' << stream_info.frame_height << ' ';
  os << '(' << stream_info.color_metadata << ')';

  return os;
}

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id) {
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

  MimeType mime_type(mime);
  if (!mime_type.is_valid()) {
    SB_LOG(WARNING) << mime << " is not a valid mime type, assuming sdr video.";
    return true;
  }
  const std::vector<std::string> codecs = mime_type.GetCodecs();
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

  SB_DCHECK(video_codec != kSbMediaVideoCodecNone);
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
  return left.codec != right.codec ||
         left.samples_per_second != right.samples_per_second ||
         left.number_of_channels != right.number_of_channels ||
         left.audio_specific_config != right.audio_specific_config;
}

int AudioDurationToFrames(SbTime duration, int samples_per_second) {
  SB_DCHECK(samples_per_second > 0)
      << "samples_per_second has to be greater than 0";
  // The same as `frames = (duration / kSbTimeSecond) * samples_per_second`,
  // switch order to avoid precision loss due to integer division.
  return duration * samples_per_second / kSbTimeSecond;
}

SbTime AudioFramesToDuration(int frames, int samples_per_second) {
  SB_DCHECK(samples_per_second > 0)
      << "samples_per_second has to be greater than 0";
  return frames * kSbTimeSecond / std::max(samples_per_second, 1);
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return memcmp(&metadata_1, &metadata_2, sizeof(SbMediaColorMetadata)) == 0;
}

bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
#if SB_API_VERSION >= 15
  const SbMediaVideoStreamInfo& stream_info_1 = sample_info_1.stream_info;
  const SbMediaVideoStreamInfo& stream_info_2 = sample_info_2.stream_info;
#else   // SB_API_VERSION >= 15
  const SbMediaVideoStreamInfo& stream_info_1 = sample_info_1;
  const SbMediaVideoStreamInfo& stream_info_2 = sample_info_2;
#endif  // SB_API_VERSION >= 15

  if (stream_info_1.codec != stream_info_2.codec) {
    return false;
  }
  if (stream_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  if (strcmp(stream_info_1.mime, stream_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(stream_info_1.max_video_capabilities,
             stream_info_2.max_video_capabilities) != 0) {
    return false;
  }

  if (sample_info_1.is_key_frame != sample_info_2.is_key_frame) {
    return false;
  }
  if (stream_info_1.frame_width != stream_info_2.frame_width) {
    return false;
  }
  if (stream_info_1.frame_height != stream_info_2.frame_height) {
    return false;
  }
  return stream_info_1.color_metadata == stream_info_2.color_metadata;
}

#if SB_API_VERSION >= 15

bool operator==(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2) {
  if (stream_info_1.codec != stream_info_2.codec) {
    return false;
  }
  if (stream_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  if (strcmp(stream_info_1.mime, stream_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(stream_info_1.max_video_capabilities,
             stream_info_2.max_video_capabilities) != 0) {
    return false;
  }
  if (stream_info_1.frame_width != stream_info_2.frame_width) {
    return false;
  }
  if (stream_info_1.frame_height != stream_info_2.frame_height) {
    return false;
  }
  return stream_info_1.color_metadata == stream_info_2.color_metadata;
}

#endif  // SB_API_VERSION >= 15

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return !(metadata_1 == metadata_2);
}

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  return !(sample_info_1 == sample_info_2);
}

#if SB_API_VERSION >= 15
bool operator!=(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2) {
  return !(stream_info_1 == stream_info_2);
}
#endif  // SB_API_VERSION >= 15
