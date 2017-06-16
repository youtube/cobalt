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
#include "starboard/thread.h"

namespace starboard {
namespace nplb {

#if SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

TEST_F(SpeechRecognizerTest, StartTestSunnyDay) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {false, false, 1};
  if (!SbSpeechRecognizerStart(recognizer, &configuration)) {
    SB_LOG(WARNING) << "SbSpeechRecognizerStart failed. Test skipped.";
    SbSpeechRecognizerDestroy(recognizer);
    return;
  }

  EXPECT_CALL(test_mock(), OnEvent()).Times(testing::AtLeast(1));
  Wait();
  SbSpeechRecognizerStop(recognizer);
  SbSpeechRecognizerDestroy(recognizer);
}

TEST_F(SpeechRecognizerTest, StartRecognizerWithContinuousRecognition) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {true, false, 1};
  if (!SbSpeechRecognizerStart(recognizer, &configuration)) {
    SB_LOG(WARNING) << "SbSpeechRecognizerStart failed. Test skipped.";
    SbSpeechRecognizerDestroy(recognizer);
    return;
  }

  EXPECT_CALL(test_mock(), OnEvent()).Times(testing::AtLeast(1));
  Wait();
  SbSpeechRecognizerStop(recognizer);
  SbSpeechRecognizerDestroy(recognizer);
}

TEST_F(SpeechRecognizerTest, StartRecognizerWithInterimResults) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {false, true, 1};
  if (!SbSpeechRecognizerStart(recognizer, &configuration)) {
    SB_LOG(WARNING) << "SbSpeechRecognizerStart failed. Test skipped.";
    SbSpeechRecognizerDestroy(recognizer);
    return;
  }

  EXPECT_CALL(test_mock(), OnEvent()).Times(testing::AtLeast(1));
  Wait();
  SbSpeechRecognizerStop(recognizer);
  SbSpeechRecognizerDestroy(recognizer);
}

TEST_F(SpeechRecognizerTest, StartRecognizerWith10MaxAlternatives) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {true, true, 10};
  if (!SbSpeechRecognizerStart(recognizer, &configuration)) {
    SB_LOG(WARNING) << "SbSpeechRecognizerStart failed. Test skipped.";
    SbSpeechRecognizerDestroy(recognizer);
    return;
  }

  EXPECT_CALL(test_mock(), OnEvent()).Times(testing::AtLeast(1));
  Wait();
  SbSpeechRecognizerStop(recognizer);
  SbSpeechRecognizerDestroy(recognizer);
}

TEST_F(SpeechRecognizerTest, StartIsCalledMultipleTimes) {
  SbSpeechRecognizer recognizer = SbSpeechRecognizerCreate(handler());
  EXPECT_TRUE(SbSpeechRecognizerIsValid(recognizer));
  SbSpeechConfiguration configuration = {true, true, 1};
  if (!SbSpeechRecognizerStart(recognizer, &configuration)) {
    SB_LOG(WARNING) << "SbSpeechRecognizerStart failed. Test skipped.";
    SbSpeechRecognizerDestroy(recognizer);
    return;
  }

  EXPECT_CALL(test_mock(), OnEvent()).Times(testing::AtLeast(1));
  bool success = SbSpeechRecognizerStart(recognizer, &configuration);
  EXPECT_FALSE(success);
  success = SbSpeechRecognizerStart(recognizer, &configuration);
  EXPECT_FALSE(success);
  Wait();
  SbSpeechRecognizerDestroy(recognizer);
}

TEST_F(SpeechRecognizerTest, StartWithInvalidSpeechRecognizer) {
  SbSpeechConfiguration configuration = {true, true, 1};
  bool success =
      SbSpeechRecognizerStart(kSbSpeechRecognizerInvalid, &configuration);
  EXPECT_FALSE(success);
}

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >= 5

}  // namespace nplb
}  // namespace starboard
