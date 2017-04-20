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

#include "cobalt/speech/starboard_speech_recognizer.h"

#if defined(SB_USE_SB_SPEECH_RECOGNIZER)

#include "starboard/log.h"

namespace cobalt {
namespace speech {

StarboardSpeechRecognizer::StarboardSpeechRecognizer(
    const EventCallback& event_callback)
    : SpeechRecognizer(event_callback) {}

StarboardSpeechRecognizer::~StarboardSpeechRecognizer() { SB_NOTIMPLEMENTED(); }

void StarboardSpeechRecognizer::Start(
    const SpeechRecognitionConfig& /*config*/) {
  SB_NOTIMPLEMENTED();
}

void StarboardSpeechRecognizer::Stop() { SB_NOTIMPLEMENTED(); }

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_SPEECH_RECOGNIZER)
