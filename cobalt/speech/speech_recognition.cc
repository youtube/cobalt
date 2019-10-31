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

#include "cobalt/speech/speech_recognition.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"

namespace cobalt {
namespace speech {

// Default values
// continuous: false.
// interim results: false.
// max alternatives: 1.
SpeechRecognition::SpeechRecognition(script::EnvironmentSettings* settings)
    : dom::EventTarget(settings),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          manager_(base::polymorphic_downcast<dom::DOMSettings*>(settings)
                       ->network_module(),
                   base::Bind(&SpeechRecognition::OnEventAvailable,
                              base::Unretained(this)),
                   base::polymorphic_downcast<dom::DOMSettings*>(settings)
                       ->microphone_options())),
      config_("" /*lang*/, false /*continuous*/, false /*interim_results*/,
              1 /*max alternatives*/) {}

void SpeechRecognition::Start(script::ExceptionState* exception_state) {
  manager_.Start(config_, exception_state);
}

void SpeechRecognition::Stop() { manager_.Stop(); }

void SpeechRecognition::Abort() { manager_.Abort(); }

bool SpeechRecognition::OnEventAvailable(
    const scoped_refptr<dom::Event>& event) {
  return DispatchEvent(event);
}

}  // namespace speech
}  // namespace cobalt
