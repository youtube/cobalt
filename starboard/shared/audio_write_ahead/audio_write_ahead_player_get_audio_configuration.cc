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

#include "starboard/shared/audio_write_ahead/audio_write_ahead_player_get_audio_configuration.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace audio_write_ahead {

bool ConfigurableAudioWriteAheadPlayerGetAudioConfiguration(
    SbPlayer player,
    int index,
    CobaltExtensionMediaAudioConfiguration* out_audio_configuration) {
  SB_DCHECK(SbPlayerIsValid(player));
  SB_DCHECK(index >= 0);
  SB_DCHECK(out_audio_configuration);

  if (index > 0) {
    // We assume that |player| only uses the primary (index 0) audio output.
    // For playbacks using more than one audio outputs, or using audio outputs
    // other than the primary one, the platform should have its own
    // `SbPlayerGetAudioConfiguration()` implementation.
    return false;
  }

  const CobaltExtensionConfigurableAudioWriteAheadApi* audio_write_ahead_api =
      static_cast<const CobaltExtensionConfigurableAudioWriteAheadApi*>(
          SbSystemGetExtension(
              kCobaltExtensionConfigurableAudioWriteAheadName));
  if (!audio_write_ahead_api) {
    return false;
  }

  SB_DCHECK(audio_write_ahead_api->name ==
            std::string(kCobaltExtensionConfigurableAudioWriteAheadName));
  SB_DCHECK(audio_write_ahead_api->version == 1u);
  SB_DCHECK(audio_write_ahead_api->MediaGetAudioConfiguration);

  return audio_write_ahead_api->MediaGetAudioConfiguration(
      index, out_audio_configuration);
}

}  // namespace audio_write_ahead
}  // namespace shared
}  // namespace starboard
