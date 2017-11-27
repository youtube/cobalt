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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_LIST_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_LIST_H_

#include <limits>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_recognition_result.h"

namespace cobalt {
namespace speech {

// The SpeechRecognitionResultList object holds a sequence of recognition
// results representing the complete return result of a continuous recognition.
// For a non-continuous recognition, it will hold only a single value.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-resultlist
class SpeechRecognitionResultList : public script::Wrappable {
 public:
  typedef std::vector<scoped_refptr<SpeechRecognitionResult> >
      SpeechRecognitionResults;

  explicit SpeechRecognitionResultList(const SpeechRecognitionResults& result);

  // Represents how many results are represented in the item array.
  uint32 length() const {
    CHECK_LE(results_.size(), std::numeric_limits<uint32>::max());
    return static_cast<uint32>(results_.size());
  }
  // The item getter returns a SpeechRecognitionResult from the index into
  // an array of result values.
  scoped_refptr<SpeechRecognitionResult> Item(uint32 index) const;

  DEFINE_WRAPPABLE_TYPE(SpeechRecognitionResultList);

 private:
  ~SpeechRecognitionResultList() override {}

  SpeechRecognitionResults results_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionResultList);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_RESULT_LIST_H_
