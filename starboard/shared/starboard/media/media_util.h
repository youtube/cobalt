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

#include "starboard/common/size.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"

namespace starboard {

// Encapsulates all information contained in `SbMediaAudioStreamInfo`.  It
// doesn't maintain the same binary layout as `SbMediaAudioStreamInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
//
// Lifetime/Ownership: Objects of this class are value types, typically owned
// by player or decoder components, and can be copied or moved.
//
// Threading Model: This class is not thread-safe and is intended to be used
// on a single thread or protected by external synchronization.
class AudioStreamInfo {
 public:
  AudioStreamInfo() = default;
  explicit AudioStreamInfo(const SbMediaAudioStreamInfo& that) { *this = that; }
  AudioStreamInfo& operator=(const SbMediaAudioStreamInfo& that);

  void ConvertTo(SbMediaAudioStreamInfo* audio_stream_info) const;

  SbMediaAudioCodec codec() const { return codec_; }
  const std::string& mime() const { return mime_; }
  uint16_t number_of_channels() const { return number_of_channels_; }
  uint32_t samples_per_second() const { return samples_per_second_; }
  void set_samples_per_second(uint32_t samples_per_second) {
    samples_per_second_ = samples_per_second;
  }
  uint16_t bits_per_sample() const { return bits_per_sample_; }
  const std::vector<uint8_t>& audio_specific_config() const {
    return audio_specific_config_;
  }

 private:
  SbMediaAudioCodec codec_ = kSbMediaAudioCodecNone;
  std::string mime_;
  uint16_t number_of_channels_ = 0;
  uint32_t samples_per_second_ = 0;
  uint16_t bits_per_sample_ = 0;
  std::vector<uint8_t> audio_specific_config_;
};

bool operator==(const AudioStreamInfo& left, const AudioStreamInfo& right);
bool operator!=(const AudioStreamInfo& left, const AudioStreamInfo& right);

std::ostream& operator<<(std::ostream& os, const AudioStreamInfo& info);

// Encapsulates all information contained in `SbMediaAudioSampleInfo`.  It
// doesn't maintain the same binary layout as `SbMediaAudioSampleInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
//
// Lifetime/Ownership: Objects of this class are value types, typically owned
// by player or decoder components, and can be copied or moved.
//
// Threading Model: This class is not thread-safe and is intended to be used
// on a single thread or protected by external synchronization.
class AudioSampleInfo {
 public:
  AudioSampleInfo() = default;
  explicit AudioSampleInfo(const SbMediaAudioSampleInfo& that) { *this = that; }
  AudioSampleInfo& operator=(const SbMediaAudioSampleInfo& that);

  void ConvertTo(SbMediaAudioSampleInfo* audio_sample_info) const;

  const AudioStreamInfo& stream_info() const { return stream_info_; }
  int64_t discarded_duration_from_front() const {
    return discarded_duration_from_front_;
  }
  int64_t discarded_duration_from_back() const {
    return discarded_duration_from_back_;
  }

 private:
  AudioStreamInfo stream_info_;
  int64_t discarded_duration_from_front_ = 0;  // in microseconds.
  int64_t discarded_duration_from_back_ = 0;   // in microseconds.
};

// Encapsulates all information contained in `SbMediaVideoStreamInfo`.  It
// doesn't maintain the same binary layout as `SbMediaVideoStreamInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
//
// Lifetime/Ownership: Objects of this class are value types, typically owned
// by player or decoder components, and can be copied or moved.
//
// Threading Model: This class is not thread-safe and is intended to be used
// on a single thread or protected by external synchronization.
class VideoStreamInfo {
 public:
  VideoStreamInfo() = default;
  explicit VideoStreamInfo(const SbMediaVideoStreamInfo& that) { *this = that; }
  VideoStreamInfo& operator=(const SbMediaVideoStreamInfo& that);

  void ConvertTo(SbMediaVideoStreamInfo* video_stream_info) const;

  SbMediaVideoCodec codec() const { return codec_; }
  const std::string& mime() const { return mime_; }
  const std::string& max_video_capabilities() const {
    return max_video_capabilities_;
  }
  Size frame_size() const { return frame_size_; }
  const SbMediaColorMetadata& color_metadata() const { return color_metadata_; }

 private:
  SbMediaVideoCodec codec_ = kSbMediaVideoCodecNone;
  std::string mime_;
  std::string max_video_capabilities_;
  Size frame_size_;
  SbMediaColorMetadata color_metadata_ = {};
};

bool operator==(const VideoStreamInfo& left, const VideoStreamInfo& right);
bool operator!=(const VideoStreamInfo& left, const VideoStreamInfo& right);

// Encapsulates all information contained in `SbMediaVideoSampleInfo`.  It
// doesn't maintain the same binary layout as `SbMediaVideoSampleInfo`, and is
// intended to be used across the codebase as a C++ wrapper that owns the memory
// of all its pointer members.
//
// Lifetime/Ownership: Objects of this class are value types, typically owned
// by player or decoder components, and can be copied or moved.
//
// Threading Model: This class is not thread-safe and is intended to be used
// on a single thread or protected by external synchronization.
class VideoSampleInfo {
 public:
  VideoSampleInfo() = default;
  explicit VideoSampleInfo(const SbMediaVideoSampleInfo& that) { *this = that; }
  VideoSampleInfo& operator=(const SbMediaVideoSampleInfo& that);

  void ConvertTo(SbMediaVideoSampleInfo* video_sample_info) const;

  const VideoStreamInfo& stream_info() const { return stream_info_; }
  bool is_key_frame() const { return is_key_frame_; }

 private:
  VideoStreamInfo stream_info_;
  bool is_key_frame_ = false;
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

// Durations are in microseconds.
int AudioDurationToFrames(int64_t duration, int samples_per_second);
int64_t AudioFramesToDuration(int frames, int samples_per_second);

int AlignUp(int value, int alignment);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
