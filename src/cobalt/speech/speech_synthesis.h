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

#ifndef COBALT_SPEECH_SPEECH_SYNTHESIS_H_
#define COBALT_SPEECH_SPEECH_SYNTHESIS_H_

#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_synthesis_utterance.h"
#include "cobalt/speech/speech_synthesis_voice.h"
#include "starboard/system.h"

namespace cobalt {

namespace dom {
// Forward declare Navigator, so that we don't pull in it's dependency on
// media_session.
class Navigator;
}  // namespace dom

namespace speech {

// The speech synthesis interface is the scripted web API for controlling a
// given speech synthesis.
//   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#tts-section
class SpeechSynthesis : public dom::EventTarget {
 public:
  typedef script::Sequence<scoped_refptr<SpeechSynthesisVoice> >
      SpeechSynthesisVoiceSequence;

  explicit SpeechSynthesis(const scoped_refptr<dom::Navigator>& navigator);

  // Readonly Attributes.
  bool pending() const { return !utterances_.empty(); }
  bool speaking() const { return false; }
  bool paused() const { return paused_; }

  // EventHandlers.
  const EventListenerScriptValue* onvoiceschanged() const {
    return GetAttributeEventListener(base::Tokens::voiceschanged());
  }

  void set_onvoiceschanged(const EventListenerScriptValue& event_listener);

  // Functions.
  void Speak(const scoped_refptr<SpeechSynthesisUtterance>& utterance);
  void Cancel();
  void Pause() { paused_ = true; }
  void Resume();
  SpeechSynthesisVoiceSequence GetVoices() const { return voices_; }

  DEFINE_WRAPPABLE_TYPE(SpeechSynthesis);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~SpeechSynthesis() override;

  void DispatchErrorEvent(
      const scoped_refptr<SpeechSynthesisUtterance>& utterance,
      SpeechSynthesisErrorCode error_code);

  bool paused_;
  typedef std::list<scoped_refptr<SpeechSynthesisUtterance> > UtterancesList;
  UtterancesList utterances_;
  SpeechSynthesisVoiceSequence voices_;

  scoped_refptr<dom::Navigator> navigator_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesis);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_SYNTHESIS_H_
