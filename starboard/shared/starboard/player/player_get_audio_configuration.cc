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

#include "starboard/player.h"

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "starboard/time.h"

#if SB_API_VERSION >= 15

bool SbPlayerGetAudioConfiguration(
    SbPlayer player,
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
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

  return SbMediaGetAudioConfiguration(index, out_audio_configuration);
}

#endif  // SB_API_VERSION >= 15
