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

#if SB_HAS(SPEECH_RECOGNIZER) && \
    SB_API_VERSION >= SB_SPEECH_RECOGNIZER_API_VERSION

class EventMock : public RefCounted<EventMock> {
 public:
  MOCK_METHOD0(OnEvent, void(void));
};

class SpeechRecognizerTest : public ::testing::Test {
 public:
  SpeechRecognizerTest()
      : handler_(), test_mock_(new ::testing::StrictMock<EventMock>()) {
    handler_.on_speech_detected = OnSpeechDetected;
    handler_.on_error = OnError;
    handler_.on_results = OnResults;
    handler_.context = this;
  }

  static void OnSpeechDetected(void* context, bool detected) {
    SpeechRecognizerTest* test = static_cast<SpeechRecognizerTest*>(context);
    test->OnSignalEvent();
  }
  static void OnError(void* context, SbSpeechRecognizerError error) {
    SpeechRecognizerTest* test = static_cast<SpeechRecognizerTest*>(context);
    test->OnSignalEvent();
  }
  static void OnResults(void* context,
                        SbSpeechResult* results,
                        int results_size,
                        bool is_final) {
    SpeechRecognizerTest* test = static_cast<SpeechRecognizerTest*>(context);
    test->OnSignalEvent();
  }

  SbSpeechRecognizerHandler* handler() { return &handler_; }

  EventMock& test_mock() { return *test_mock_.get(); }

  void Wait() {
    if (!event_semaphore_.TakeWait(kWaitTime)) {
      SB_LOG(WARNING) << "Waiting for recognizer event to come, but timeout!";
    }
  }

  void OnSignalEvent() {
    test_mock_->OnEvent();
    event_semaphore_.Put();
  }

 protected:
  // Per test teardown.
  virtual void TearDown() {
    // Wait for the speech recognizer server to tear down in order to start
    // a new one for the next test. The speech recognizer server was running in
    // another thread.
    SbThreadSleep(kTearDownTime);
  }

 private:
  const SbTime kTearDownTime = 10 * kSbTimeMillisecond;
  const SbTime kWaitTime = 600 * kSbTimeMillisecond;

  SbSpeechRecognizerHandler handler_;

  starboard::Semaphore event_semaphore_;

  const scoped_refptr<EventMock> test_mock_;
};

#endif  // SB_HAS(SPEECH_RECOGNIZER) && SB_API_VERSION >=
        // SB_SPEECH_RECOGNIZER_API_VERSION

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_SPEECH_RECOGNIZER_HELPER_H_
