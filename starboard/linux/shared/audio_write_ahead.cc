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

#include "starboard/linux/shared/audio_write_ahead.h"

#include "cobalt/extension/audio_write_ahead.h"
#include "starboard/common/log.h"
#include "starboard/linux/shared/audio_write_ahead_player_get_audio_configuration.h"

// Omit namespace linux due to symbol name conflict.

namespace starboard {
namespace shared {

namespace {

const CobaltExtensionConfigurableAudioWriteAheadApi
    kConfigurableAudioWriteAheadApi = {
        kCobaltExtensionConfigurableAudioWriteAheadName,
        1,
        &ConfigurableAudioWriteAheadPlayerGetAudioConfiguration,
};

}  // namespace

const void* GetConfigurableAudioWriteAheadApi() {
  SB_LOG(INFO) << "Audio write ahead extension enabled.";
  return &kConfigurableAudioWriteAheadApi;
}

}  // namespace shared
}  // namespace starboard
