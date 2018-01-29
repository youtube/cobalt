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

#ifndef COBALT_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_
#define COBALT_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_

#include "base/basictypes.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_synthesis_error_code.h"
#include "cobalt/speech/speech_synthesis_event.h"

namespace cobalt {
namespace speech {

// The SpeechSynthesisErrorEvent is the interface used for the
// SpeechSynthesisUtterance error event.
//   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#tts-section
class SpeechSynthesisErrorEvent : public SpeechSynthesisEvent {
 public:
  explicit SpeechSynthesisErrorEvent(
      SpeechSynthesisErrorCode error_code,
      const scoped_refptr<SpeechSynthesisUtterance>& utterance)
      : SpeechSynthesisEvent(base::Tokens::error(), utterance),
        error_code_(error_code) {}

  SpeechSynthesisErrorCode error() const { return error_code_; }

  DEFINE_WRAPPABLE_TYPE(SpeechSynthesisErrorEvent);

 private:
  ~SpeechSynthesisErrorEvent() override {}

  const SpeechSynthesisErrorCode error_code_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisErrorEvent);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_SYNTHESIS_ERROR_EVENT_H_
