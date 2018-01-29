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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_H_

#include <string>

#include "cobalt/dom/event_target.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognition_manager.h"

namespace cobalt {
namespace speech {

// The speech recognition interface is the scripted web API for controlling a
// given recognition.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-section
class SpeechRecognition : public dom::EventTarget {
 public:
  explicit SpeechRecognition(script::EnvironmentSettings* settings);

  // When the start method is called, it represents the moment in time the web
  // application wishes to begin recognition.
  void Start(script::ExceptionState* exception_state);
  // The stop method represents an instruction to the recognition service to
  // stop listening to more audio, and to try and return a result using just the
  // audio that it has already received for this recognition.
  void Stop();
  // The abort method is a request to immediately stop listening and stop
  // recognizing. It does not return any information.
  void Abort();

  const std::string& lang() const { return config_.lang; }
  void set_lang(const std::string& lang) { config_.lang = lang; }

  bool continuous() const { return config_.continuous; }
  void set_continuous(bool continuous) { config_.continuous = continuous; }

  bool interim_results() const { return config_.interim_results; }
  void set_interim_results(bool interim_results) {
    config_.interim_results = interim_results;
  }

  uint32 max_alternatives() const { return config_.max_alternatives; }
  void set_max_alternatives(uint32 max_alternatives) {
    config_.max_alternatives = max_alternatives;
  }

  // Fired when some sound, possibly speech, has been detected.
  const EventListenerScriptValue* onsoundstart() const {
    return GetAttributeEventListener(base::Tokens::soundstart());
  }
  void set_onsoundstart(const EventListenerScriptValue& listener) {
    SetAttributeEventListener(base::Tokens::soundstart(), listener);
  }

  // Fired when some sound is no longer detected.
  const EventListenerScriptValue* onsoundend() const {
    return GetAttributeEventListener(base::Tokens::soundend());
  }
  void set_onsoundend(const EventListenerScriptValue& listener) {
    SetAttributeEventListener(base::Tokens::soundend(), listener);
  }

  // Fired when the speech recognizer returns a result.
  const EventListenerScriptValue* onresult() const {
    return GetAttributeEventListener(base::Tokens::result());
  }
  void set_onresult(const EventListenerScriptValue& listener) {
    SetAttributeEventListener(base::Tokens::result(), listener);
  }

  // Fired when the speech recognizer returns a final result with no recognition
  // hypothesis that meet or exceed the confidence threshold.
  const EventListenerScriptValue* onnomatch() const {
    return GetAttributeEventListener(base::Tokens::nomatch());
  }
  void set_onnomatch(const EventListenerScriptValue& listener) {
    SetAttributeEventListener(base::Tokens::nomatch(), listener);
  }

  // Fired when the speech recognition error occurs.
  const EventListenerScriptValue* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptValue& listener) {
    SetAttributeEventListener(base::Tokens::error(), listener);
  }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognition);

 private:
  ~SpeechRecognition() override {}

  // Callback from recognition manager.
  bool OnEventAvailable(const scoped_refptr<dom::Event>& event);

  // Handles main operations of speech recognition including audio encoding,
  // mic audio retrieving and audio data recognizing.
  SpeechRecognitionManager manager_;
  // Store the speech recognition configurations.
  SpeechRecognitionConfig config_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognition);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_H_
