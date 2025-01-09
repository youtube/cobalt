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

#include "starboard/audio_sink.h"

#include "starboard/common/log.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

void SbAudioSinkDestroy(SbAudioSink audio_sink) {
  using ::starboard::shared::starboard::audio_sink::SbAudioSinkImpl;

  if (audio_sink == kSbAudioSinkInvalid) {
    return;
  }

  SbAudioSinkPrivate::Type* type = SbAudioSinkImpl::GetPrimaryType();
  if (type && type->IsValid(audio_sink)) {
    type->Destroy(audio_sink);
    return;
  }
  type = SbAudioSinkImpl::GetFallbackType();
  if (type && type->IsValid(audio_sink)) {
    type->Destroy(audio_sink);
    return;
  }

  SB_LOG(WARNING) << "Invalid audio sink.";
}
