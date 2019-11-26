// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_CHANNEL_LAYOUT_MIXER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_CHANNEL_LAYOUT_MIXER_H_

#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class AudioChannelLayoutMixer {
 public:
  typedef ::starboard::shared::starboard::player::DecodedAudio DecodedAudio;

  virtual ~AudioChannelLayoutMixer() {}

  virtual scoped_refptr<DecodedAudio> Mix(
      const scoped_refptr<DecodedAudio>& audio_data) = 0;

  static scoped_ptr<AudioChannelLayoutMixer> Create(
      SbMediaAudioSampleType sample_type,
      SbMediaAudioFrameStorageType storage_type,
      int output_channels);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_CHANNEL_LAYOUT_MIXER_H_
