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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_EVENT_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_EVENT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_recognition_result_list.h"

namespace cobalt {
namespace speech {

// The SpeechRecognitionEvent is the event that is raised each time there are
// any changes to interim or final results.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-event
class SpeechRecognitionEvent : public dom::Event {
 public:
  // Result event and nomatch event MUST use the SpeechRecognitionEvent
  // interface.
  enum Type { kResult, kNoMatch };

  SpeechRecognitionEvent(Type type, uint32 result_index,
                         const scoped_refptr<SpeechRecognitionResultList>&
                             speech_recognition_result_list);

  uint32 result_index() { return result_index_; }
  scoped_refptr<SpeechRecognitionResultList> results() const {
    return speech_recognition_result_list_;
  }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognitionEvent);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~SpeechRecognitionEvent() override {}

  // The result index MUST be set to the lowest index in the "results" array
  // that has changed.
  uint32 result_index_;
  // The array of all current recognition results for this session.
  scoped_refptr<SpeechRecognitionResultList> speech_recognition_result_list_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionEvent);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_EVENT_H_
