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

#include "cobalt/speech/speech_synthesis_utterance.h"

namespace cobalt {
namespace speech {

SpeechSynthesisUtterance::SpeechSynthesisUtterance()
    : volume_(1.0f), rate_(1.0f), pitch_(1.0f), pending_speak_(false) {}
SpeechSynthesisUtterance::SpeechSynthesisUtterance(const std::string& text)
    : text_(text),
      volume_(1.0f),
      rate_(1.0f),
      pitch_(1.0f),
      pending_speak_(false) {}

SpeechSynthesisUtterance::SpeechSynthesisUtterance(
    const scoped_refptr<SpeechSynthesisUtterance>& utterance)
    : text_(utterance->text_),
      lang_(utterance->lang_),
      voice_(utterance->voice_),
      volume_(utterance->volume_),
      rate_(utterance->rate_),
      pitch_(utterance->pitch_),
      pending_speak_(false) {
  set_onstart(EventListenerScriptValue::Reference(this, *utterance->onstart())
                  .referenced_value());
  set_onend(EventListenerScriptValue::Reference(this, *utterance->onend())
                .referenced_value());
  set_onerror(EventListenerScriptValue::Reference(this, *utterance->onerror())
                  .referenced_value());
  set_onpause(EventListenerScriptValue::Reference(this, *utterance->onpause())
                  .referenced_value());
  set_onresume(EventListenerScriptValue::Reference(this, *utterance->onresume())
                   .referenced_value());
  set_onmark(EventListenerScriptValue::Reference(this, *utterance->onmark())
                 .referenced_value());
  set_onboundary(
      EventListenerScriptValue::Reference(this, *utterance->onboundary())
          .referenced_value());
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
