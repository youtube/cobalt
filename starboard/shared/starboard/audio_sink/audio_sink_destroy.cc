// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "starboard/audio_sink.h"
// clang-format on

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

void SbAudioSinkDestroy(SbAudioSink audio_sink) {
  if (audio_sink == kSbAudioSinkInvalid) {
    return;
  }
  // TODO: b/565505891 - Remove this legacy path once all sink types clean up in
  // their destructors.
  if (SbAudioSinkPrivate::Type* type = audio_sink->GetType()) {
    type->Destroy(audio_sink);
    return;
  }
  delete audio_sink;
}
