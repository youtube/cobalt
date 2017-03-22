// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/speech_synthesis.h"

#if SB_API_VERSION < SB_EXPERIMENTAL_API_VERSION
// DEPRECATED IN API VERSION 4

#include "starboard/shared/speechd/speechd_internal.h"

using starboard::shared::speechd::SpeechDispatcher;

bool SbSpeechSynthesisSetLanguage(const char* lang) {
  SpeechDispatcher* speech_dispatcher = SpeechDispatcher::Get();
  if (!speech_dispatcher) {
    return false;
  }
  return speech_dispatcher->SetLanguage(lang);
}

#endif
