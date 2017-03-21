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

#include "cobalt/accessibility/starboard_tts_engine.h"

#include "starboard/speech_synthesis.h"

#if SB_HAS(SPEECH_SYNTHESIS)

namespace cobalt {
namespace accessibility {

void StarboardTTSEngine::SpeakNow(const std::string& text) OVERRIDE {
  SbSpeechSynthesisCancel();
  Speak(text);
}

void StarboardTTSEngine::Speak(const std::string& text) OVERRIDE {
  SbSpeechSynthesisSpeak(text.c_str());
}

}  // namespace accessibility
}  // namespace cobalt

#endif  // SB_HAS(SPEECH_SYNTHESIS)
