// Copyright 2023 The Cobalt Authors.All Rights Reserved.
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

#include "starboard/shared/audio_write_ahead/audio_write_ahead_media_get_audio_configuration.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/media.h"

namespace starboard {
namespace shared {
namespace audio_write_ahead {

bool ConfigurableAudioWriteAheadMediaGetAudioConfiguration(
    int output_index,
    CobaltExtensionMediaAudioConfiguration* out_configuration) {
  SB_DCHECK(output_index >= 0);
  SB_DCHECK(out_configuration);

  if (output_index != 0) {
    return false;
  }

  *out_configuration = {};

  out_configuration->connector = kCobaltExtensionMediaAudioConnectorUnknown;
  out_configuration->latency = 0;
  out_configuration->coding_type = kSbMediaAudioCodingTypePcm;
  out_configuration->number_of_channels = SbAudioSinkGetMaxChannels();
  return true;
}

}  // namespace audio_write_ahead
}  // namespace shared
}  // namespace starboard
