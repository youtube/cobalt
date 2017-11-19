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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_H_

#include <limits>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_recognition_alternative.h"

namespace cobalt {
namespace speech {

// The speech recognition result object represents a single one-shot recognition
// match, either as one small part of a continuous recognition or as the
// complete return result of a non-continuous recognition.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-result
class SpeechRecognitionResult : public script::Wrappable {
 public:
  typedef std::vector<scoped_refptr<SpeechRecognitionAlternative> >
      SpeechRecognitionAlternatives;

  SpeechRecognitionResult(const SpeechRecognitionAlternatives& alternatives,
                          bool final);

  // Represents how many n-best alternatives are represented in the item array.
  uint32 length() const {
    CHECK_LE(alternatives_.size(), std::numeric_limits<uint32>::max());
    return static_cast<uint32>(alternatives_.size());
  }
  // The item getter returns a SpeechRecognitionAlternative from the index into
  // an array of n-best values.
  scoped_refptr<SpeechRecognitionAlternative> Item(uint32 index) const;
  // The final MUST be set to true if this is the final time the speech service
  // will return this particular index value. If the value is false, then this
  // represents an interim result that could still be changed.
  bool is_final() const { return final_; }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognitionResult);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  SpeechRecognitionAlternatives alternatives_;
  bool final_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionResult);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_H_
