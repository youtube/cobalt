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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_PLAYER_COMPONENTS_FACTORY_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_PLAYER_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/filter/player_components.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class StubPlayerComponentsFactory : public PlayerComponents::Factory {
 public:
  static std::unique_ptr<PlayerComponents::Factory> Create();

  SubComponents CreateSubComponents(
      const CreationParameters& creation_parameters) override {
    AudioComponents audio_components;
    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      audio_components = CreateStubAudioComponents(creation_parameters);
    }
    VideoComponents video_components;
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      video_components = CreateStubVideoComponents(creation_parameters);
    }
    return {.audio = std::move(audio_components),
            .video = std::move(video_components)};
  }

 private:
  StubPlayerComponentsFactory() = default;

  // Disallow copy and assign.
  StubPlayerComponentsFactory(const StubPlayerComponentsFactory&) = delete;
  void operator=(const StubPlayerComponentsFactory&) = delete;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_PLAYER_COMPONENTS_FACTORY_H_
