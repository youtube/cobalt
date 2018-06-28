// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_ACCESSIBILITY_TTS_ENGINE_H_
#define COBALT_ACCESSIBILITY_TTS_ENGINE_H_

#include <string>

namespace cobalt {
namespace accessibility {
// Interface for a TTS (text-to-speech) implementation.
class TTSEngine {
 public:
  // Speaks the contents of |text|, pre-empting any utterances that are
  // currently in progress.
  virtual void SpeakNow(const std::string& text) = 0;
  // Queues the contents of |text| to be spoken after any in-progress utterances
  // are finished.
  virtual void Speak(const std::string& text) = 0;
  virtual ~TTSEngine() {}
};
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_TTS_ENGINE_H_
