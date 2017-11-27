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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_ALTERNATIVE_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_ALTERNATIVE_H_

#include <string>

#include "base/basictypes.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace speech {

// The SpeechRecognitionAlternative represents a simple view of the response
// that gets used in a n-best list.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-alternative
class SpeechRecognitionAlternative : public script::Wrappable {
 public:
  SpeechRecognitionAlternative(const std::string& transcript, float confidence);

  const std::string& transcript() const { return transcript_; }
  float confidence() const { return confidence_; }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognitionAlternative);

 private:
  ~SpeechRecognitionAlternative() override {}

  // The transcript string represents the raw words that the user spoke.
  std::string transcript_;
  // The confidence represents a numeric estimate between 0 and 1 of how
  // confident the recognition system is that the recognition is correct. A
  // higher number means the system is more confident.
  float confidence_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionAlternative);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_ALTERNATIVE_H_
