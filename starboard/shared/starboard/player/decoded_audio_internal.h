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

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/buffer_internal.h"

namespace starboard {

// Decoded audio frames produced by an audio decoder.  It can contain multiple
// frames with continuous timestamps.
class DecodedAudio : public RefCountedThreadSafe<DecodedAudio> {
 public:
  DecodedAudio();  // Signal an EOS.
  // TODO(b/272837615): Remove `storage_type` support and always store data in
  // interleaved. Refactor the places that store sample in planar to convert the
  // samples to interleaved on creation.
  DecodedAudio(int channels,
               SbMediaAudioSampleType sample_type,
               SbMediaAudioFrameStorageType storage_type,
               int64_t timestamp,
               int size_in_bytes);

  DecodedAudio(int channels,
               SbMediaAudioSampleType sample_type,
               SbMediaAudioFrameStorageType storage_type,
               int64_t timestamp,
               int size_in_bytes,
               Buffer&& storage);

  int channels() const { return channels_; }
  SbMediaAudioSampleType sample_type() const { return sample_type_; }
  SbMediaAudioFrameStorageType storage_type() const { return storage_type_; }

  bool is_end_of_stream() const { return channels_ == 0; }
  int64_t timestamp() const { return timestamp_; }
  const uint8_t* data() const { return storage_.data() + offset_in_bytes_; }
  const int16_t* data_as_int16() const {
    return reinterpret_cast<const int16_t*>(storage_.data() + offset_in_bytes_);
  }
  const float* data_as_float32() const {
    return reinterpret_cast<const float*>(storage_.data() + offset_in_bytes_);
  }
  int size_in_bytes() const { return size_in_bytes_; }

  uint8_t* data() { return storage_.data() + offset_in_bytes_; }
  int16_t* data_as_int16() {
    return reinterpret_cast<int16_t*>(storage_.data() + offset_in_bytes_);
  }
  float* data_as_float32() {
    return reinterpret_cast<float*>(storage_.data() + offset_in_bytes_);
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

  bool IsFormat(SbMediaAudioSampleType sample_type,
                SbMediaAudioFrameStorageType storage_type) const;
  scoped_refptr<DecodedAudio> SwitchFormatTo(
      SbMediaAudioSampleType new_sample_type,
      SbMediaAudioFrameStorageType new_storage_type) const
      SB_WARN_UNUSED_RESULT;

  scoped_refptr<DecodedAudio> Clone() const;

 private:
  scoped_refptr<DecodedAudio> SwitchSampleTypeTo(
      SbMediaAudioSampleType new_sample_type) const;
  scoped_refptr<DecodedAudio> SwitchStorageTypeTo(
      SbMediaAudioFrameStorageType new_storage_type) const;

  const int channels_;
  const SbMediaAudioSampleType sample_type_;
  const SbMediaAudioFrameStorageType storage_type_;
  // The timestamp of the first audio frame in microseconds.
  int64_t timestamp_;
  Buffer storage_;
  // The audio samples to be played are stored in the memory region starts from
  // `storage_.data() + offset_in_bytes_`, `size_in_bytes_` bytes in total.
  int offset_in_bytes_ = 0;
  int size_in_bytes_ = 0;

  DecodedAudio(const DecodedAudio&) = delete;
  void operator=(const DecodedAudio&) = delete;
};

bool operator==(const DecodedAudio& left, const DecodedAudio& right);
bool operator!=(const DecodedAudio& left, const DecodedAudio& right);

// For debugging or testing only.
std::ostream& operator<<(std::ostream& os, const DecodedAudio& decoded_audio);

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_DECODED_AUDIO_INTERNAL_H_
