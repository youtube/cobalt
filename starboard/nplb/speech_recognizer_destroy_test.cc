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

#include "starboard/nplb/speech_recognizer_helper.h"
#include "starboard/speech_recognizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

TEST_F(SpeechRecognizerTest, DestroyInvalidSpeechRecognizer) {
  SbSpeechRecognizerDestroy(kSbSpeechRecognizerInvalid);
}

TEST_F(SpeechRecognizerTest, DestroyRecognizerWithoutStopping) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {true, true, 1};
  SbSpeechRecognizerStart(recognizer, &configuration);
  SbSpeechRecognizerDestroy(recognizer);
}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

}  // namespace nplb
}  // namespace starboard
