//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "starboard/media.h"
#include "starboard/audio_sink.h"

#include <cstring>

#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

using namespace third_party::starboard::rdk::shared;

bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  bool ret = platform::device().audio_configuration(output_index, out_audio_configuration).value_or(false);
  if (!ret && output_index == 0 && out_audio_configuration) {
    memset(out_audio_configuration, 0, sizeof(SbMediaAudioConfiguration));
    out_audio_configuration->coding_type = kSbMediaAudioCodingTypePcm;
    out_audio_configuration->number_of_channels = SbAudioSinkGetMaxChannels();
    ret = true;
  }
  return ret;
}
