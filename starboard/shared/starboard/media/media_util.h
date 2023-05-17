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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_

#include <ostream>
#include <string>
#include <vector>

#include "starboard/extension/enhanced_audio.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// Encapsulates all information contained in `SbMediaAudioStreamInfo`.  It
// doesn't maintain the same binary layout as `SbMediaAudioStreamInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
struct AudioStreamInfo {
  AudioStreamInfo() = default;
  template <typename StreamInfo>
  explicit AudioStreamInfo(const StreamInfo& that) {
    *this = that;
  }
  AudioStreamInfo& operator=(const SbMediaAudioStreamInfo& that);
  AudioStreamInfo& operator=(
      const CobaltExtensionEnhancedAudioMediaAudioStreamInfo& that);

  void ConvertTo(SbMediaAudioStreamInfo* audio_stream_info) const;
  void ConvertTo(CobaltExtensionEnhancedAudioMediaAudioStreamInfo*
                     audio_stream_info) const;

  // The member variables are the C++ mappings of the members of
  // `SbMediaAudioStreamInfo` defined in `media.h`.  Please refer to the comment
  // of `SbMediaAudioStreamInfo` for more details.
  SbMediaAudioCodec codec = kSbMediaAudioCodecNone;
  std::string mime;
  uint16_t number_of_channels;
  uint32_t samples_per_second;
  uint16_t bits_per_sample;
  std::vector<uint8_t> audio_specific_config;
};

bool operator==(const AudioStreamInfo& left, const AudioStreamInfo& right);
bool operator!=(const AudioStreamInfo& left, const AudioStreamInfo& right);

// Encapsulates all information contained in `SbMediaAudioSampleInfo`.  It
// doesn't maintain the same binary layout as `SbMediaAudioSampleInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
struct AudioSampleInfo {
  AudioSampleInfo() = default;
  template <typename SampleInfo>
  explicit AudioSampleInfo(const SampleInfo& that) {
    *this = that;
  }
  AudioSampleInfo& operator=(const SbMediaAudioSampleInfo& that);
  AudioSampleInfo& operator=(
      const CobaltExtensionEnhancedAudioMediaAudioSampleInfo& that);

  void ConvertTo(SbMediaAudioSampleInfo* audio_sample_info) const;
  void ConvertTo(CobaltExtensionEnhancedAudioMediaAudioSampleInfo*
                     audio_sample_info) const;

  // The member variables are the C++ mappings of the members of
  // `SbMediaAudioSampleInfo` defined in `media.h`.  Please refer to the comment
  // of `SbMediaAudioSampleInfo` for more details.
  AudioStreamInfo stream_info;
  SbTime discarded_duration_from_front = 0;
  SbTime discarded_duration_from_back = 0;
};

// Encapsulates all information contained in `SbMediaVideoStreamInfo`.  It
// doesn't maintain the same binary layout as `SbMediaVideoStreamInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
struct VideoStreamInfo {
  VideoStreamInfo() = default;
  template <typename StreamInfo>
  explicit VideoStreamInfo(const StreamInfo& that) {
    *this = that;
  }
  VideoStreamInfo& operator=(const SbMediaVideoStreamInfo& that);
  VideoStreamInfo& operator=(
      const CobaltExtensionEnhancedAudioMediaVideoStreamInfo& that);

  void ConvertTo(SbMediaVideoStreamInfo* video_stream_info) const;
  void ConvertTo(CobaltExtensionEnhancedAudioMediaVideoStreamInfo*
                     video_stream_info) const;

  // The member variables are the C++ mappings of the members of
  // `SbMediaVideoStreamInfo` defined in `media.h`.  Please refer to the comment
  // of `SbMediaVideoStreamInfo` for more details.
  SbMediaVideoCodec codec = kSbMediaVideoCodecNone;
  std::string mime;
  std::string max_video_capabilities;
  int frame_width;
  int frame_height;
  SbMediaColorMetadata color_metadata;
};

bool operator==(const VideoStreamInfo& left, const VideoStreamInfo& right);
bool operator!=(const VideoStreamInfo& left, const VideoStreamInfo& right);

// Encapsulates all information contained in `SbMediaVideoSampleInfo`.  It
// doesn't maintain the same binary layout as `SbMediaVideoSampleInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
struct VideoSampleInfo {
  VideoSampleInfo() = default;
  template <typename SampleInfo>
  explicit VideoSampleInfo(const SampleInfo& that) {
    *this = that;
  }
  VideoSampleInfo& operator=(const SbMediaVideoSampleInfo& that);
  VideoSampleInfo& operator=(
      const CobaltExtensionEnhancedAudioMediaVideoSampleInfo& that);

  void ConvertTo(SbMediaVideoSampleInfo* video_sample_info) const;
  void ConvertTo(CobaltExtensionEnhancedAudioMediaVideoSampleInfo*
                     video_sample_info) const;

  // The member variables are the C++ mappings of the members of
  // `SbMediaVideoSampleInfo` defined in `media.h`.  Please refer to the comment
  // of `SbMediaVideoSampleInfo` for more details.
  VideoStreamInfo stream_info;
  bool is_key_frame;
};

std::ostream& operator<<(std::ostream& os, const VideoSampleInfo& stream_info);

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id);
bool IsSDRVideo(const char* mime);

int GetBytesPerSample(SbMediaAudioSampleType sample_type);

std::string GetStringRepresentation(const uint8_t* data, const int size);
std::string GetMixedRepresentation(const uint8_t* data,
                                   const int size,
                                   const int bytes_per_line);

//  When this function returns true, usually indicates that the two sample info
//  cannot be processed by the same audio decoder.
bool IsAudioSampleInfoSubstantiallyDifferent(const AudioStreamInfo& left,
                                             const AudioStreamInfo& right);

int AudioDurationToFrames(SbTime duration, int samples_per_second);
SbTime AudioFramesToDuration(int frames, int samples_per_second);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

#if SB_API_VERSION >= 15
bool operator==(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2);
#endif  // SB_API_VERSION >= 15

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

#if SB_API_VERSION >= 15
bool operator!=(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2);
#endif  // SB_API_VERSION >= 15

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
