// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_SPEECH_RECOGNIZER_HELPER_H_
#define STARBOARD_NPLB_SPEECH_RECOGNIZER_HELPER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/common/semaphore.h"
#include "starboard/speech_recognizer.h"
#include "starboard/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

#if SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)

class SpeechRecognizerTest : public ::testing::Test {
 public:
  SpeechRecognizerTest() : handler_() {
    handler_.on_speech_detected = OnSpeechDetected;
    handler_.on_error = OnError;
    handler_.on_results = OnResults;
    handler_.context = this;
  }

  static void OnSpeechDetected(void* context, bool detected) {}
  static void OnError(void* context, SbSpeechRecognizerError error) {}
  static void OnResults(void* context,
                        SbSpeechResult* results,
                        int results_size,
                        bool is_final) {}

  SbSpeechRecognizerHandler* handler() {
    return &handler_;
  }

 protected:
  bool isTestFixtureSupported;
  virtual void SetUp() {
    // We include all API tests at compile time after Starboard version 12, so
    // we must do a runtime check to determine whether or not that API (and
    // thus the test fixture) is supported.
#if SB_API_VERSION >= 12
    isTestFixtureSupported = SbSpeechRecognizerIsSupported();
#else
    isTestFixtureSupported = true;
#endif
  }

  // TODO: Use GTEST_SKIP in |SetUp| when we have a newer version of gtest.
  bool SkipLocale() {
    if (!isTestFixtureSupported) {
      SB_LOG(INFO) << "Speech recognizer not supported. Test skipped.";
    }
    return !isTestFixtureSupported;
  }

  // Per test teardown.
  virtual void TearDown() {
    // Wait for the speech recognizer server to tear down in order to start
    // a new one for the next test. The speech recognizer server was running in
    // another thread.
    SbThreadSleep(kTearDownTime);
  }

 private:
  const SbTime kTearDownTime = 10 * kSbTimeMillisecond;

  SbSpeechRecognizerHandler handler_;
};

#endif  // SB_API_VERSION >= 12 || SB_HAS(SPEECH_RECOGNIZER)

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SPEECH_RECOGNIZER_HELPER_H_
