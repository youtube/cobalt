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

// Default implementation of SbMediaGetAudioConfiguration().

#include "starboard/media.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"

bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  SB_DCHECK(output_index >= 0);
  SB_DCHECK(out_configuration);

  if (output_index != 0) {
    return false;
  }

  *out_configuration = {};

#if SB_API_VERSION >= 15
  out_configuration->connector = kSbMediaAudioConnectorUnknown;
#else   // SB_API_VERSION >= 15
  out_configuration->index = 0;
  out_configuration->connector = kSbMediaAudioConnectorNone;
#endif  // SB_API_VERSION >= 15
  out_configuration->latency = 0;
  out_configuration->coding_type = kSbMediaAudioCodingTypePcm;
  out_configuration->number_of_channels = SbAudioSinkGetMaxChannels();
  return true;
}
