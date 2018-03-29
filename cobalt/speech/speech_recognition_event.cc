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

#include "cobalt/speech/speech_recognition_event.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace speech {

namespace {
base::Token TypeEnumToToken(SpeechRecognitionEvent::Type type) {
  switch (type) {
    case SpeechRecognitionEvent::kResult:
      return base::Tokens::result();
    case SpeechRecognitionEvent::kNoMatch:
      return base::Tokens::nomatch();
  }
  NOTREACHED() << "Invalid SpeechRecognitionEvent::Type";
  return base::Tokens::nomatch();
}
}  // namespace

SpeechRecognitionEvent::SpeechRecognitionEvent(
    Type type, uint32 result_index,
    const scoped_refptr<SpeechRecognitionResultList>&
        speech_recognition_result_list)
    : dom::Event(TypeEnumToToken(type)),
      result_index_(result_index),
      speech_recognition_result_list_(speech_recognition_result_list) {}

void SpeechRecognitionEvent::TraceMembers(script::Tracer* tracer) {
  dom::Event::TraceMembers(tracer);

  tracer->Trace(speech_recognition_result_list_);
}

}  // namespace speech
}  // namespace cobalt
