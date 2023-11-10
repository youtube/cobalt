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

#include "starboard/linux/shared/audio_write_ahead_player_get_audio_configuration.h"

#include <string>

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/system.h"

// Omit namespace linux due to symbol name conflict.

namespace starboard {
namespace shared {

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

  *out_audio_configuration = {};

  out_audio_configuration->connector =
      kCobaltExtensionMediaAudioConnectorUnknown;
  out_audio_configuration->latency = 0;
  out_audio_configuration->coding_type = kSbMediaAudioCodingTypePcm;
  out_audio_configuration->number_of_channels = SbAudioSinkGetMaxChannels();

  return true;
}

}  // namespace shared
}  // namespace starboard
