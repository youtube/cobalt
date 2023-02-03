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

#include <cctype>

#include "starboard/character.h"
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

}  // namespace

AudioSampleInfo::AudioSampleInfo() {
  memset(this, 0, sizeof(SbMediaAudioSampleInfo));
  codec = kSbMediaAudioCodecNone;
}

AudioSampleInfo::AudioSampleInfo(const SbMediaAudioSampleInfo& that) {
  *this = that;
}

AudioSampleInfo& AudioSampleInfo::operator=(
    const SbMediaAudioSampleInfo& that) {
  *static_cast<SbMediaAudioSampleInfo*>(this) = that;
  if (audio_specific_config_size > 0) {
    audio_specific_config_storage.resize(audio_specific_config_size);
    memcpy(audio_specific_config_storage.data(), audio_specific_config,
           audio_specific_config_size);
    audio_specific_config = audio_specific_config_storage.data();
  }

  if (codec == kSbMediaAudioCodecNone) {
    mime_storage.clear();
  } else {
    SB_DCHECK(that.mime);
    mime_storage = that.mime;
  }
  mime = mime_storage.c_str();

  return *this;
}

VideoSampleInfo::VideoSampleInfo() {
  memset(this, 0, sizeof(SbMediaVideoSampleInfo));
  codec = kSbMediaVideoCodecNone;
}

VideoSampleInfo::VideoSampleInfo(const SbMediaVideoSampleInfo& that) {
  *this = that;
}

VideoSampleInfo& VideoSampleInfo::operator=(
    const SbMediaVideoSampleInfo& that) {
  *static_cast<SbMediaVideoSampleInfo*>(this) = that;

  if (codec == kSbMediaVideoCodecNone) {
    mime_storage.clear();
    max_video_capabilities_storage.clear();
  } else {
    SB_DCHECK(that.mime);
    mime_storage = that.mime;
    max_video_capabilities_storage = that.max_video_capabilities;
  }
  mime = mime_storage.c_str();
  max_video_capabilities = max_video_capabilities_storage.c_str();

  return *this;
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

bool IsAudioSampleInfoSubstantiallyDifferent(
    const SbMediaAudioSampleInfo& left,
    const SbMediaAudioSampleInfo& right) {
  return left.codec != right.codec ||
         left.samples_per_second != right.samples_per_second ||
         left.number_of_channels != right.number_of_channels ||
         left.audio_specific_config_size != right.audio_specific_config_size ||
         memcmp(left.audio_specific_config, right.audio_specific_config,
                left.audio_specific_config_size) != 0;
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
  if (sample_info_1.codec != sample_info_2.codec) {
    return false;
  }
  if (sample_info_1.codec == kSbMediaVideoCodecNone) {
    return true;
  }

  if (strcmp(sample_info_1.mime, sample_info_2.mime) != 0) {
    return false;
  }
  if (strcmp(sample_info_1.max_video_capabilities,
             sample_info_2.max_video_capabilities) != 0) {
    return false;
  }

  if (sample_info_1.is_key_frame != sample_info_2.is_key_frame) {
    return false;
  }
  if (sample_info_1.frame_width != sample_info_2.frame_width) {
    return false;
  }
  if (sample_info_1.frame_height != sample_info_2.frame_height) {
    return false;
  }
  return sample_info_1.color_metadata == sample_info_2.color_metadata;
}

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2) {
  return !(metadata_1 == metadata_2);
}

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2) {
  return !(sample_info_1 == sample_info_2);
}
