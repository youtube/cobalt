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
  static scoped_ptr<PlayerComponents::Factory> Create();

  bool CreateSubComponents(
      const CreationParameters& creation_parameters,
      scoped_ptr<AudioDecoder>* audio_decoder,
      scoped_ptr<AudioRendererSink>* audio_renderer_sink,
      scoped_ptr<VideoDecoder>* video_decoder,
      scoped_ptr<VideoRenderAlgorithm>* video_render_algorithm,
      scoped_refptr<VideoRendererSink>* video_renderer_sink,
      std::string* error_message) override {
    SB_DCHECK(error_message);

    if (creation_parameters.audio_codec() != kSbMediaAudioCodecNone) {
      CreateStubAudioComponents(creation_parameters, audio_decoder,
                                audio_renderer_sink);
    }
    if (creation_parameters.video_codec() != kSbMediaVideoCodecNone) {
      CreateStubVideoComponents(creation_parameters, video_decoder,
                                video_render_algorithm, video_renderer_sink);
    }
    return true;
  }

 private:
  StubPlayerComponentsFactory() {}

  StubPlayerComponentsFactory(const StubPlayerComponentsFactory&) = delete;
  void operator=(const StubPlayerComponentsFactory&) = delete;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_PLAYER_COMPONENTS_FACTORY_H_
