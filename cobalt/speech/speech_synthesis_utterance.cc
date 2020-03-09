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

#include "cobalt/speech/speech_synthesis_utterance.h"

namespace cobalt {
namespace speech {

SpeechSynthesisUtterance::SpeechSynthesisUtterance(
    script::EnvironmentSettings* settings)
    : dom::EventTarget(settings),
      settings_(settings),
      volume_(1.0f),
      rate_(1.0f),
      pitch_(1.0f),
      pending_speak_(false) {}
SpeechSynthesisUtterance::SpeechSynthesisUtterance(
    script::EnvironmentSettings* settings, const std::string& text)
    : dom::EventTarget(settings),
      settings_(settings),
      text_(text),
      volume_(1.0f),
      rate_(1.0f),
      pitch_(1.0f),
      pending_speak_(false) {}

SpeechSynthesisUtterance::SpeechSynthesisUtterance(
    const scoped_refptr<SpeechSynthesisUtterance>& utterance)
    : dom::EventTarget(utterance->settings_),
      text_(utterance->text_),
      lang_(utterance->lang_),
      voice_(utterance->voice_),
      volume_(utterance->volume_),
      rate_(utterance->rate_),
      pitch_(utterance->pitch_),
      pending_speak_(false) {
  using ListenerSetterFunctionType = base::Callback<void(
      SpeechSynthesisUtterance*,
      const cobalt::dom::EventTarget::EventListenerScriptValue&)>;
  auto ListenerCopyHelper = [this](
                                const EventListenerScriptValue* script_value,
                                const ListenerSetterFunctionType& setter_func) {
    if (script_value) {
      setter_func.Run(this, *script_value);
    }
  };
  ListenerCopyHelper(utterance->onstart(),
                     base::Bind(&SpeechSynthesisUtterance::set_onstart));
  ListenerCopyHelper(utterance->onend(),
                     base::Bind(&SpeechSynthesisUtterance::set_onend));
  ListenerCopyHelper(utterance->onerror(),
                     base::Bind(&SpeechSynthesisUtterance::set_onerror));
  ListenerCopyHelper(utterance->onpause(),
                     base::Bind(&SpeechSynthesisUtterance::set_onpause));
  ListenerCopyHelper(utterance->onresume(),
                     base::Bind(&SpeechSynthesisUtterance::set_onresume));
  ListenerCopyHelper(utterance->onmark(),
                     base::Bind(&SpeechSynthesisUtterance::set_onmark));
  ListenerCopyHelper(utterance->onboundary(),
                     base::Bind(&SpeechSynthesisUtterance::set_onboundary));
}

void SpeechSynthesisUtterance::DispatchErrorCancelledEvent() {
  if (pending_speak_) {
    DLOG(INFO) << "Utterance has a pending Speak()";
    DispatchErrorEvent(kSpeechSynthesisErrorCodeCanceled);
  }
}

void SpeechSynthesisUtterance::SignalPendingSpeak() { pending_speak_ = true; }

void SpeechSynthesisUtterance::DispatchStartEvent() {
  DispatchEvent(new SpeechSynthesisEvent(base::Tokens::start(), this));
}

void SpeechSynthesisUtterance::DispatchEndEvent() {
  pending_speak_ = false;
  DispatchEvent(new SpeechSynthesisEvent(base::Tokens::end(), this));
}

void SpeechSynthesisUtterance::DispatchErrorEvent(
    SpeechSynthesisErrorCode error_code) {
  pending_speak_ = false;
  DispatchEvent(new SpeechSynthesisErrorEvent(error_code, this));
}

void SpeechSynthesisUtterance::TraceMembers(script::Tracer* tracer) {
  dom::EventTarget::TraceMembers(tracer);

  tracer->Trace(voice_);
}

SpeechSynthesisUtterance::~SpeechSynthesisUtterance() {
  DCHECK(!pending_speak_) << "Destructed utterance has a pending Speak()";
}

}  // namespace speech
}  // namespace cobalt
