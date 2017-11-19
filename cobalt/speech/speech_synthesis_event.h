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

#ifndef COBALT_SPEECH_SPEECH_SYNTHESIS_EVENT_H_
#define COBALT_SPEECH_SPEECH_SYNTHESIS_EVENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace speech {

class SpeechSynthesisUtterance;

// The speech synthesis event interface is the scripted web API for defining a
// speech synthesis event.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechsynthesisevent
class SpeechSynthesisEvent : public dom::Event {
 public:
  SpeechSynthesisEvent(
      base::Token event_name,
      const scoped_refptr<SpeechSynthesisUtterance>& utterance);

  ~SpeechSynthesisEvent();

  // Readonly Attributes.
  const scoped_refptr<SpeechSynthesisUtterance>& utterance() const {
    return utterance_;
  }
  const base::optional<unsigned int> char_index() const { return char_index_; }
  const base::optional<float> elapsed_time() const { return elapsed_time_; }
  const base::optional<std::string>& name() const { return name_; }

  DEFINE_WRAPPABLE_TYPE(SpeechSynthesisEvent);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  const scoped_refptr<SpeechSynthesisUtterance> utterance_;
  const base::optional<unsigned int> char_index_;
  const base::optional<float> elapsed_time_;
  const base::optional<std::string> name_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSynthesisEvent);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_SYNTHESIS_EVENT_H_
