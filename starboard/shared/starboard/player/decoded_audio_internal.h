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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_DECODED_AUDIO_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_DECODED_AUDIO_INTERNAL_H_

#include <ostream>
#include <type_traits>

#include "starboard/common/span.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/buffer_internal.h"

namespace starboard {

// Decoded audio frames produced by an audio decoder.  It can contain multiple
// frames with continuous timestamps.
class DecodedAudio {
 public:
  static DecodedAudio CreateEOSBuffer();
  DecodedAudio(int channels,
               SbMediaAudioSampleType sample_type,
               int64_t timestamp,
               int size_in_bytes);

  DecodedAudio(int channels,
               SbMediaAudioSampleType sample_type,
               int64_t timestamp,
               int size_in_bytes,
               Buffer&& storage);

  // Move-only semantics
  DecodedAudio(DecodedAudio&& other) noexcept;
  DecodedAudio& operator=(DecodedAudio&& other) noexcept;

  // Disable copy and assignment.
  DecodedAudio(const DecodedAudio&) = delete;
  DecodedAudio& operator=(const DecodedAudio&) = delete;

  int channels() const { return channels_; }
  SbMediaAudioSampleType sample_type() const { return sample_type_; }

  bool is_end_of_stream() const { return channels_ == 0; }
  int64_t timestamp() const { return timestamp_; }
  const uint8_t* data() const { return storage_.data() + offset_in_bytes_; }
  int size_in_bytes() const { return size_in_bytes_; }

  uint8_t* data() { return storage_.data() + offset_in_bytes_; }

  template <typename T = uint8_t>
  span<const T> as_span() const {
    static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, int16_t> ||
                  std::is_same_v<T, float>);
    if constexpr (std::is_same_v<T, int16_t>) {
      SB_CHECK_EQ(sample_type_, kSbMediaAudioSampleTypeInt16Deprecated);
    } else if constexpr (std::is_same_v<T, float>) {
      SB_CHECK_EQ(sample_type_, kSbMediaAudioSampleTypeFloat32);
    }
    span<const uint8_t> bytes =
        storage_.as_span().subspan(static_cast<size_t>(offset_in_bytes_),
                                   static_cast<size_t>(size_in_bytes_));
    SB_CHECK_EQ(bytes.size() % sizeof(T), size_t{0});
    return {reinterpret_cast<const T*>(bytes.data()), bytes.size() / sizeof(T)};
  }

  template <typename T = uint8_t>
  span<T> as_span() {
    span<const T> s = static_cast<const DecodedAudio*>(this)->as_span<T>();
    return {const_cast<T*>(s.data()), s.size()};
  }

  int frames() const;

  void ShrinkTo(int new_size_in_bytes);

  // During seeking, the target time can be in the middle of the DecodedAudio
  // object.  This function will adjust the object to the seek target time by
  // removing the frames in the beginning that are before the seek target time.
  void AdjustForSeekTime(int sample_rate, int64_t seeking_to_time);
  void AdjustForDiscardedDurations(int sample_rate,
                                   int64_t discarded_duration_from_front,
                                   int64_t discarded_duration_from_back);

  // During format switching, this method can perform sample type conversions
  // on-the-fly.
  DecodedAudio SwitchFormatTo(SbMediaAudioSampleType new_sample_type) const
      SB_WARN_UNUSED_RESULT;

  DecodedAudio CloneForTesting() const;

 private:
  DecodedAudio();

  DecodedAudio SwitchSampleTypeTo(SbMediaAudioSampleType new_sample_type) const;

  int channels_;
  SbMediaAudioSampleType sample_type_;
  // The timestamp of the first audio frame in microseconds.
  int64_t timestamp_;
  Buffer storage_;
  // The audio samples to be played are stored in the memory region starts from
  // `storage_.data() + offset_in_bytes_`, `size_in_bytes_` bytes in total.
  int offset_in_bytes_ = 0;
  int size_in_bytes_ = 0;
};

bool operator==(const DecodedAudio& left, const DecodedAudio& right);
bool operator!=(const DecodedAudio& left, const DecodedAudio& right);

// For debugging or testing only.
std::ostream& operator<<(std::ostream& os, const DecodedAudio& decoded_audio);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_DECODED_AUDIO_INTERNAL_H_
