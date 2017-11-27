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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_

#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"

#include "testing/gmock/include/gmock/gmock.h"

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder(SbMediaAudioSampleType sample_type,
                   SbMediaAudioFrameStorageType storage_type,
                   int sample_per_second)
      : sample_type_(sample_type),
        storage_type_(storage_type),
        samples_per_second_(sample_per_second) {}

  MOCK_METHOD2(Initialize, void(const Closure&, const Closure&));
  MOCK_METHOD2(Decode, void(const scoped_refptr<InputBuffer>&, const Closure&));
  MOCK_METHOD0(WriteEndOfStream, void());
  MOCK_METHOD0(Read, scoped_refptr<DecodedAudio>());
  MOCK_METHOD0(Reset, void());

  SbMediaAudioSampleType GetSampleType() const override {
    return sample_type_;
  }
  SbMediaAudioFrameStorageType GetStorageType() const override {
    return storage_type_;
  }
  int GetSamplesPerSecond() const override { return samples_per_second_; }

 private:
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;
  int samples_per_second_;
};

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_
