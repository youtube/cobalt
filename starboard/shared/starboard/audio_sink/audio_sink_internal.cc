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

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/stub_audio_sink_type.h"
#include "starboard/shared/starboard/command_line.h"

namespace {

bool is_fallback_to_stub_enabled;
SbAudioSinkPrivate::Type* primary_audio_sink_type;
SbAudioSinkPrivate::Type* fallback_audio_sink_type;

// Command line switch that controls whether we default to the stub audio sink,
// even when the primary audio sink may be available.
const char kUseStubAudioSink[] = "use_stub_audio_sink";

}  // namespace

using starboard::shared::starboard::audio_sink::StubAudioSinkType;

// static
void SbAudioSinkPrivate::Initialize() {
  fallback_audio_sink_type = new StubAudioSinkType;
  PlatformInitialize();
}

// static
void SbAudioSinkPrivate::TearDown() {
  PlatformTearDown();
  delete fallback_audio_sink_type;
  fallback_audio_sink_type = NULL;
}

// static
void SbAudioSinkPrivate::SetPrimaryType(Type* type) {
  primary_audio_sink_type = type;
}

// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetPrimaryType() {
  return primary_audio_sink_type;
}
// static
void SbAudioSinkPrivate::EnableFallbackToStub() {
  is_fallback_to_stub_enabled = true;
}
// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetFallbackType() {
  if (is_fallback_to_stub_enabled) {
    return fallback_audio_sink_type;
  }
  return NULL;
}

// static
SbAudioSinkPrivate::Type* SbAudioSinkPrivate::GetPreferredType() {
  SbAudioSinkPrivate::Type* audio_sink_type = NULL;
  auto command_line =
      starboard::shared::starboard::Application::Get()->GetCommandLine();
  if (!command_line->HasSwitch(kUseStubAudioSink)) {
    audio_sink_type = SbAudioSinkPrivate::GetPrimaryType();
  }
  if (!audio_sink_type) {
    SB_LOG(WARNING) << "Primary audio sink type not selected or missing, "
                       "opting to use Fallback instead.";
    audio_sink_type = SbAudioSinkPrivate::GetFallbackType();
  }
  if (audio_sink_type == NULL) {
    SB_LOG(WARNING) << "Fallback audio sink type is not enabled.";
  }
  return audio_sink_type;
}
