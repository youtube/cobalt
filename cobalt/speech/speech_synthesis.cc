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

#include "cobalt/speech/speech_synthesis.h"

#include <string>

#include "cobalt/dom/navigator.h"
#include "starboard/speech_synthesis.h"

namespace cobalt {
namespace speech {

bool SpeechSynthesis::SpeechSynthesisIsSupported() {
  return SbSpeechSynthesisIsSupported();
}

SpeechSynthesis::SpeechSynthesis(script::EnvironmentSettings* settings,
                                 const scoped_refptr<dom::Navigator>& navigator,
                                 bool log_output)
    : web::EventTarget(settings),
      log_output_(log_output),
      paused_(false),
      navigator_(navigator) {
  if (SpeechSynthesis::SpeechSynthesisIsSupported()) {
    const char* kVoiceName = "Cobalt";
    std::string voice_urn(kVoiceName);
    std::string voice_lang(navigator_->language());
    voice_urn.append(" ");
    voice_urn.append(voice_lang);
    voices_.push_back(new SpeechSynthesisVoice(voice_urn, kVoiceName,
                                               voice_lang, false, true));
  }
}

SpeechSynthesis::~SpeechSynthesis() {}

void SpeechSynthesis::set_onvoiceschanged(
    const EventListenerScriptValue& event_listener) {
  base_token::Token event_name = base::Tokens::voiceschanged();
  SetAttributeEventListener(event_name, event_listener);
  DispatchEvent(new web::Event(event_name));
}

void SpeechSynthesis::Cancel() {
  for (UtterancesList::iterator utterance_iterator = utterances_.begin();
       utterance_iterator != utterances_.end(); ++utterance_iterator) {
    (*utterance_iterator)->DispatchErrorCancelledEvent();
  }
  utterances_.clear();
  if (SpeechSynthesis::SpeechSynthesisIsSupported()) {
    SbSpeechSynthesisCancel();
  }
}

void SpeechSynthesis::Resume() {
  if (paused_) {
    paused_ = false;
    for (UtterancesList::iterator utterance_iterator = utterances_.begin();
         utterance_iterator != utterances_.end(); ++utterance_iterator) {
      Speak(*utterance_iterator);
      if (utterances_.empty()) break;
    }
  }
}

void SpeechSynthesis::TraceMembers(script::Tracer* tracer) {
  web::EventTarget::TraceMembers(tracer);

  tracer->TraceItems(utterances_);
  tracer->TraceItems(voices_);
  tracer->Trace(navigator_);
}

void SpeechSynthesis::DispatchErrorEvent(
    const scoped_refptr<SpeechSynthesisUtterance>& utterance,
    SpeechSynthesisErrorCode error_code) {
  utterance->DispatchErrorEvent(error_code);
  Cancel();
}

void SpeechSynthesis::Speak(
    const scoped_refptr<SpeechSynthesisUtterance>& utterance) {
  if (log_output_) {
    LOG(INFO) << "JavaScript Text-to-speech: " << utterance->text();
  }
  if (paused_) {
    // When the synthesis is paused, the utterance needs to be added to a queue
    // and preserved until synthesis is canceled or resumed.
    // A copy of the utterance needs to be made, so that the current state of
    // the object is preserved.
    SpeechSynthesisUtterance* copied_utterance =
        new SpeechSynthesisUtterance(utterance);
    copied_utterance->SignalPendingSpeak();
    utterances_.push_back(copied_utterance);
    return;
  }
  utterance->SignalPendingSpeak();
  if (SpeechSynthesis::SpeechSynthesisIsSupported()) {
    if (!utterance->lang().empty() &&
        utterance->lang() != navigator_->language()) {
      DispatchErrorEvent(utterance,
                         kSpeechSynthesisErrorCodeLanguageUnavailable);
      return;
    }
    if ((utterance->volume() != 1.0f) || (utterance->rate() != 1.0f) ||
        (utterance->pitch() != 1.0f)) {
      DispatchErrorEvent(utterance, kSpeechSynthesisErrorCodeInvalidArgument);
      return;
    }

    SB_DLOG(INFO) << "Speaking: \"" << utterance->text() << "\" "
                  << utterance->lang();
    SbSpeechSynthesisSpeak(utterance->text().c_str());
    utterance->DispatchStartEvent();
    utterance->DispatchEndEvent();
    return;
  }
  DispatchErrorEvent(utterance, kSpeechSynthesisErrorCodeSynthesisUnavailable);
}

}  // namespace speech
}  // namespace cobalt
