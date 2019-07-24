// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/shared/alsa/alsa_audio_sink_type.h"
#include "starboard/shared/pulse/pulse_audio_sink_type.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace {
bool is_fallback_to_alsa = false;
}

// static
void SbAudioSinkPrivate::PlatformInitialize() {
  starboard::shared::pulse::PlatformInitialize();
  if (GetPrimaryType()) {
    SB_LOG(INFO) << "Use PulseAudio";
  } else {
    SB_LOG(INFO) << "Use ALSA";
    starboard::shared::alsa::PlatformInitialize();
    is_fallback_to_alsa = true;
  }
  SbAudioSinkPrivate::EnableFallbackToStub();
}

// static
void SbAudioSinkPrivate::PlatformTearDown() {
  if (is_fallback_to_alsa) {
    starboard::shared::alsa::PlatformTearDown();
  } else {
    starboard::shared::pulse::PlatformTearDown();
  }
}
