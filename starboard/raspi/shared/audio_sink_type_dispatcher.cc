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

#include "starboard/shared/alsa/alsa_audio_sink_type.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace audio_sink {

// static
void SbAudioSinkImpl::PlatformInitialize() {
  ::starboard::shared::alsa::PlatformInitialize();
  SbAudioSinkPrivate::EnableFallbackToStub();
}

// static
void SbAudioSinkImpl::PlatformTearDown() {
  ::starboard::shared::alsa::PlatformTearDown();
}

}  // namespace audio_sink
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
