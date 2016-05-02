/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_H_

#include <string>

#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace speech {

// The speech recognition interface is the scripted web API for controlling a
// given recognition.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-section
class SpeechRecognition : public dom::EventTarget {
 public:
  SpeechRecognition();

  // When the start method is called, it represents the moment in time the web
  // application wishes to begin recognition.
  void Start();
  // The stop method represents an instruction to the recognition service to
  // stop listening to more audio, and to try and return a result using just the
  // audio that it has already received for this recognition.
  void Stop();
  // The abort method is a request to immediately stop listening and stop
  // recognizing. It does not return any information.
  void Abort();

  const std::string& lang() const { return lang_; }
  void set_lang(const std::string& lang) { lang_ = lang; }

  bool continuous() const { return continuous_; }
  void set_continuous(bool continuous) { continuous_ = continuous; }

  bool interim_results() const { return interim_results_; }
  void set_interim_results(bool interim_results) {
    interim_results_ = interim_results;
  }

  uint32 max_alternatives() const { return max_alternatives_; }
  void set_max_alternatives(uint32 max_alternatives) {
    max_alternatives_ = max_alternatives;
  }

  // Fired when some sound, possibly speech, has been detected.
  const EventListenerScriptObject* onsoundstart() const {
    return GetAttributeEventListener(base::Tokens::soundstart());
  }
  void set_onsoundstart(const EventListenerScriptObject& listener) {
    SetAttributeEventListener(base::Tokens::soundstart(), listener);
  }

  // Fired when some sound is no longer detected.
  const EventListenerScriptObject* onsoundend() const {
    return GetAttributeEventListener(base::Tokens::soundend());
  }
  void set_onsoundend(const EventListenerScriptObject& listener) {
    SetAttributeEventListener(base::Tokens::soundend(), listener);
  }

  // Fired when the speech recognizer returns a result.
  const EventListenerScriptObject* onresult() const {
    return GetAttributeEventListener(base::Tokens::result());
  }
  void set_onresult(const EventListenerScriptObject& listener) {
    SetAttributeEventListener(base::Tokens::result(), listener);
  }

  // Fired when the speech recognizer returns a final result with no recognition
  // hypothesis that meet or exceed the confidence threshold.
  const EventListenerScriptObject* onnomatch() const {
    return GetAttributeEventListener(base::Tokens::nomatch());
  }
  void set_onnomatch(const EventListenerScriptObject& listener) {
    SetAttributeEventListener(base::Tokens::nomatch(), listener);
  }

  // Fired when the speech recognition error occurs.
  const EventListenerScriptObject* onerror() const {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptObject& listener) {
    SetAttributeEventListener(base::Tokens::error(), listener);
  }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognition);

 private:
  ~SpeechRecognition() OVERRIDE {}

  // This attribute will set the language of the recognition for the request.
  std::string lang_;
  // When the continuous attribute is set to false, the user agent MUST return
  // no more than one final result in response to starting recognition. When
  // the continuous attribute is set to true, the user agent MUST return zero
  // or more final results representing multiple consecutive recognitions in
  // response to starting recognition. The default value MUST be false.
  bool continuous_;
  // Controls whether interim results are returned. The default value MUST be
  // false.
  bool interim_results_;
  // This attribute will set the maximum number of SpeechRecognitionAlternatives
  // per result. The default value is 1.
  uint32 max_alternatives_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognition);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_H_
