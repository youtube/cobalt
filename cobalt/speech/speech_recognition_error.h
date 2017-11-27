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

#ifndef COBALT_SPEECH_SPEECH_RECOGNITION_ERROR_H_
#define COBALT_SPEECH_SPEECH_RECOGNITION_ERROR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/speech_recognition_error_code.h"

namespace cobalt {
namespace speech {

// The SpeechRecognitionError event is the interface used for the error event.
//   https://dvcs.w3.org/hg/speech-api/raw-file/9a0075d25326/speechapi.html#speechreco-error
class SpeechRecognitionError : public dom::Event {
 public:
  SpeechRecognitionError(SpeechRecognitionErrorCode error_code,
                         const std::string& message);

  SpeechRecognitionErrorCode error() const { return error_code_; }
  const std::string& message() const { return message_; }

  DEFINE_WRAPPABLE_TYPE(SpeechRecognitionError);

 private:
  ~SpeechRecognitionError() override {}

  const SpeechRecognitionErrorCode error_code_;
  const std::string message_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionError);
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNITION_ERROR_H_
