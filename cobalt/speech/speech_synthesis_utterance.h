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

#ifndef COBALT_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_
#define COBALT_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_

#include <string>

#include "cobalt/dom/event_target.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_synthesis_error_event.h"
#include "cobalt/speech/speech_synthesis_voice.h"

namespace cobalt {
namespace speech {

// The speech synthesis voice interface is the scripted web API for defining a
// speech synthesis voice.
//   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#speechsynthesisvoice
class SpeechSynthesisUtterance : public dom::EventTarget {
 public:
  SpeechSynthesisUtterance();
  SpeechSynthesisUtterance(
      const scoped_refptr<SpeechSynthesisUtterance>& utterance);
  explicit SpeechSynthesisUtterance(const std::string& text);

  // Web API: SpeechSynthesisUtterance
  //

  // Read+Write Attributes.
  std::string text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; }

  std::string lang() const { return lang_; }
  void set_lang(const std::string& lang) { lang_ = lang; }

  scoped_refptr<SpeechSynthesisVoice> voice() const { return voice_; }
  void set_voice(const scoped_refptr<SpeechSynthesisVoice>& voice) {
    voice_ = voice;
  }

  float volume() const { return volume_; }
  void set_volume(const float volume) { volume_ = volume; }

  float rate() const { return rate_; }
  void set_rate(const float rate) { rate_ = rate; }

  float pitch() const { return pitch_; }
  void set_pitch(const float pitch) { pitch_ = pitch; }

  // EventHandlers.
  const EventListenerScriptValue* onstart() const {
    return GetAttributeEventListener(base::Tokens::start());
  }

  void set_onstart(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::start(), event_listener);
  }

  const EventListenerScriptValue* onend() const {
    return GetAttributeEventListener(base::Tokens::end());
  }

  void set_onend(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::end(), event_listener);
  }

  const EventListenerScriptValue* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }

  void set_onerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptValue* onpause() const {
    return GetAttributeEventListener(base::Tokens::pause());
  }

  void set_onpause(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::pause(), event_listener);
  }

  const EventListenerScriptValue* onresume() const {
    return GetAttributeEventListener(base::Tokens::resume());
  }

  void set_onresume(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::resume(), event_listener);
  }

  const EventListenerScriptValue* onmark() const {
    return GetAttributeEventListener(base::Tokens::mark());
  }

  void set_onmark(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::mark(), event_listener);
  }

  const EventListenerScriptValue* onboundary() const {
    return GetAttributeEventListener(base::Tokens::boundary());
  }

  void set_onboundary(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::boundary(), event_listener);
  }

  // Custom, not in any spec.
  //
  void SignalPendingSpeak();
  void DispatchErrorCancelledEvent();

  void DispatchStartEvent();
  void DispatchEndEvent();
  void DispatchErrorEvent(SpeechSynthesisErrorCode error_code);

  DEFINE_WRAPPABLE_TYPE(SpeechSynthesisUtterance);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~SpeechSynthesisUtterance() override;

  std::string text_;
  std::string lang_;
  scoped_refptr<SpeechSynthesisVoice> voice_;

  float volume_;
  float rate_;
  float pitch_;

  bool pending_speak_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisUtterance);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_SYNTHESIS_UTTERANCE_H_
