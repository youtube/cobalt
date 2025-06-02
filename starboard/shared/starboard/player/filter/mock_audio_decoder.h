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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_

#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"

#include "testing/gmock/include/gmock/gmock.h"

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/types.h"

namespace starboard::shared::starboard::player::filter::testing {

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder(SbMediaAudioSampleType sample_type,
                   SbMediaAudioFrameStorageType storage_type,
                   int samples_per_second) {}

  MOCK_METHOD2(Initialize, void(const OutputCB&, const ErrorCB&));
  MOCK_METHOD2(Decode, void(const InputBuffers&, const ConsumedCB&));
  MOCK_METHOD0(WriteEndOfStream, void());
  MOCK_METHOD1(Read, scoped_refptr<DecodedAudio>(int*));
  MOCK_METHOD0(Reset, void());
};

}  // namespace starboard::shared::starboard::player::filter::testing

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MOCK_AUDIO_DECODER_H_
