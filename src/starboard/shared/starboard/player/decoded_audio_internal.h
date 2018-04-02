// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// Decoded audio frames produced by an audio decoder.  It can contain multiple
// frames with continuous timestamps.
// It doesn't have a specific storage type and sample type.  The decoder and the
// renderer will determinate the proper storage type and sample type in their
// own way.
class DecodedAudio : public RefCountedThreadSafe<DecodedAudio> {
 public:
  DecodedAudio();  // Signal an EOS.
  DecodedAudio(int channels,
               SbMediaAudioSampleType sample_type,
               SbMediaAudioFrameStorageType storage_type,
               SbTime timestamp,
               size_t size);

  int channels() const { return channels_; }
  SbMediaAudioSampleType sample_type() const { return sample_type_; }
  SbMediaAudioFrameStorageType storage_type() const { return storage_type_; }

  bool is_end_of_stream() const { return buffer_ == NULL; }
  SbTime timestamp() const { return timestamp_; }
  const uint8_t* buffer() const { return buffer_.get(); }
  size_t size() const { return size_; }

  uint8_t* buffer() { return buffer_.get(); }
  int frames() const;

  void SwitchFormatTo(SbMediaAudioSampleType new_sample_type,
                      SbMediaAudioFrameStorageType new_storage_type);
  void ShrinkTo(size_t new_size);

 private:
  void SwitchSampleTypeTo(SbMediaAudioSampleType new_sample_type);
  void SwitchStorageTypeTo(SbMediaAudioFrameStorageType new_storage_type);

  int channels_;
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;
  // The timestamp of the first audio frame.
  SbTime timestamp_;
  // Use scoped_array<uint8_t> instead of std::vector<uint8_t> to avoid wasting
  // time on setting content to 0.
  scoped_array<uint8_t> buffer_;
  size_t size_;

  SB_DISALLOW_COPY_AND_ASSIGN(DecodedAudio);
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_DECODED_AUDIO_INTERNAL_H_
