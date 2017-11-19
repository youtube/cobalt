// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/speech/speech_recognition_result.h"

namespace cobalt {
namespace speech {

SpeechRecognitionResult::SpeechRecognitionResult(
    const SpeechRecognitionAlternatives& alternatives, bool final)
    : alternatives_(alternatives), final_(final) {}

scoped_refptr<SpeechRecognitionAlternative> SpeechRecognitionResult::Item(
    uint32 index) const {
  if (index >= length()) {
    DLOG(WARNING) << "Requested index " << index
                  << " but the length of alternatives is " << length();
    return NULL;
  }

  return alternatives_[index];
}

void SpeechRecognitionResult::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(alternatives_);
}

}  // namespace speech
}  // namespace cobalt
