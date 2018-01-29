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

#ifndef COBALT_ACCESSIBILITY_TTS_LOGGER_H_
#define COBALT_ACCESSIBILITY_TTS_LOGGER_H_

#include <string>

#include "base/logging.h"
#include "cobalt/accessibility/tts_engine.h"

namespace cobalt {
namespace accessibility {
// TTSEngine implementation that just logs the text.
class TTSLogger : public TTSEngine {
 public:
  void SpeakNow(const std::string& text) override { LogText(text); }
  void Speak(const std::string& text) override { LogText(text); }

 private:
  void LogText(const std::string& text) {
    LOG(INFO) << "Text-to-speech: " << text;
  }
  virtual ~TTSLogger() {}
};
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_TTS_LOGGER_H_
