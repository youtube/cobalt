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

#include "cobalt/speech/speech_recognizer.h"

namespace cobalt {
namespace speech {

SpeechRecognizer::SpeechRecognizer(const EventCallback& event_callback)
    : event_callback_(event_callback) {}

void SpeechRecognizer::RunEventCallback(
    const scoped_refptr<dom::Event>& event) {
  event_callback_.Run(event);
}

}  // namespace speech
}  // namespace cobalt
