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

#ifndef STARBOARD_SHARED_AUDIO_WRITE_AHEAD_AUDIO_WRITE_AHEAD_MEDIA_GET_AUDIO_CONFIGURATION_H_
#define STARBOARD_SHARED_AUDIO_WRITE_AHEAD_AUDIO_WRITE_AHEAD_MEDIA_GET_AUDIO_CONFIGURATION_H_

#include "cobalt/extension/audio_write_ahead.h"

namespace starboard {
namespace shared {
namespace audio_write_ahead {

bool ConfigurableAudioWriteAheadMediaGetAudioConfiguration(
    int output_index,
    CobaltExtensionMediaAudioConfiguration* out_configuration);

}  // namespace audio_write_ahead
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_AUDIO_WRITE_AHEAD_AUDIO_WRITE_AHEAD_MEDIA_GET_AUDIO_CONFIGURATION_H_
