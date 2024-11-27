// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/enhanced_audio/enhanced_audio.h"

#include "starboard/common/log.h"
#include "starboard/extension/enhanced_audio.h"
#include "starboard/shared/enhanced_audio/enhanced_audio_player_write_samples.h"

namespace starboard {
namespace shared {
namespace enhanced_audio {

namespace {

const CobaltExtensionEnhancedAudioApi kEnhancedAudioApi = {
    kCobaltExtensionEnhancedAudioName,
    1,
    &EnhancedAudioPlayerWriteSamples,
};

}  // namespace

const void* GetEnhancedAudioApi() {
  SB_LOG(INFO) << "EnhancedAudio extension enabled.";
  return &kEnhancedAudioApi;
}

}  // namespace enhanced_audio
}  // namespace shared
}  // namespace starboard
