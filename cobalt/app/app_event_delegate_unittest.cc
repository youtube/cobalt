// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/app/app_event_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cobalt/app/app_event_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::Return;

namespace cobalt {

class MockAppEventRunner : public AppEventRunner {
 public:
  MOCK_METHOD(void, InitializeSystem, (), (override));
  MOCK_METHOD(void,
              CreateMainDelegate,
              (absl::optional<int64_t> startup_timestamp,
               bool is_visible,
               const char* initial_deep_link),
              (override));
  MOCK_METHOD(cobalt::CobaltMainDelegate*, GetMainDelegate, (), (override));
  MOCK_METHOD(int, Run, (content::ContentMainParams params), (override));
  MOCK_METHOD(void, ShutDown, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (const, override));
  MOCK_METHOD(bool, IsVisible, (), (const, override));
  MOCK_METHOD(void, SetStateForTesting, (bool, bool), (override));

  MOCK_METHOD(void, OnStart, (const SbEvent*), (override));
  MOCK_METHOD(void, OnStop, (), (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void, OnFocus, (), (override));
  MOCK_METHOD(void, OnConceal, (), (override));
  MOCK_METHOD(void, OnReveal, (), (override));
  MOCK_METHOD(void, OnFreeze, (), (override));
  MOCK_METHOD(void, OnUnfreeze, (), (override));

  MOCK_METHOD(void, OnInput, (const SbEvent*), (override));
  MOCK_METHOD(void, OnLink, (const SbEvent*), (override));
  MOCK_METHOD(void, OnLowMemory, (const SbEvent*), (override));
  MOCK_METHOD(void, OnWindowSizeChanged, (const SbEvent*), (override));
  MOCK_METHOD(void,
              OnOsNetworkConnectedDisconnected,
              (const SbEvent*),
              (override));
  MOCK_METHOD(void,
              OnDateTimeConfigurationChanged,
              (const SbEvent*),
              (override));
  MOCK_METHOD(void,
              OnAccessibilityTextToSpeechSettingsChanged,
              (const SbEvent*),
              (override));
};

class AppEventDelegateTest : public testing::Test {
 public:
  AppEventDelegateTest() {
    auto runner = std::make_unique<MockAppEventRunner>();
    runner_ = runner.get();

    // Set up default behaviors to track state.
    ON_CALL(*runner_, IsRunning()).WillByDefault(Invoke([this]() {
      return is_running_;
    }));
    ON_CALL(*runner_, OnStart(_)).WillByDefault(Invoke([this](const SbEvent*) {
      is_running_ = true;
    }));
    ON_CALL(*runner_, OnStop()).WillByDefault(Invoke([this]() {
      is_running_ = false;
    }));
    ON_CALL(*runner_, IsVisible()).WillByDefault(Invoke([this]() {
      return is_visible_;
    }));
    ON_CALL(*runner_, OnReveal()).WillByDefault(Invoke([this]() {
      is_visible_ = true;
    }));
    ON_CALL(*runner_, OnConceal()).WillByDefault(Invoke([this]() {
      is_visible_ = false;
    }));

    delegate_ = std::make_unique<AppEventDelegate>(std::move(runner));
  }

 protected:
  void SendEvent(SbEventType type, void* data = nullptr) {
    SbEvent event = {type, 0, data};
    delegate_->HandleEvent(&event);
  }

  bool is_running_ = false;
  bool is_visible_ = false;
  raw_ptr<MockAppEventRunner> runner_;
  std::unique_ptr<AppEventDelegate> delegate_;
};

TEST_F(AppEventDelegateTest, StartVisible) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypeStart);
  EXPECT_TRUE(delegate_->IsRunning());
}

TEST_F(AppEventDelegateTest, StartPreloaded) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypePreload);
  EXPECT_TRUE(delegate_->IsRunning());
}

TEST_F(AppEventDelegateTest, LinearityBlur) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, OnStart(_));
    EXPECT_CALL(*runner_, OnReveal());
    EXPECT_CALL(*runner_, OnBlur());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeBlur);
}

TEST_F(AppEventDelegateTest, SynthesisFocusFromStopped) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, OnStart(_));  // Implicit preload
    EXPECT_CALL(*runner_, OnReveal());
    EXPECT_CALL(*runner_, OnFocus());
  }

  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, SynthesisFocusFromPreload) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypePreload);

  {
    InSequence s;
    EXPECT_CALL(*runner_, OnReveal());
    EXPECT_CALL(*runner_, OnFocus());
  }

  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, SynthesisFreezeFromStarted) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypeStart);

  {
    InSequence s;
    EXPECT_CALL(*runner_, OnBlur());
    EXPECT_CALL(*runner_, OnConceal());
    EXPECT_CALL(*runner_, OnFreeze());
  }

  SendEvent(kSbEventTypeFreeze);
}

TEST_F(AppEventDelegateTest, SynthesisStopFromStarted) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypeStart);

  {
    InSequence s;
    EXPECT_CALL(*runner_, OnBlur());
    EXPECT_CALL(*runner_, OnConceal());
    EXPECT_CALL(*runner_, OnFreeze());
    EXPECT_CALL(*runner_, OnStop());
  }

  SendEvent(kSbEventTypeStop);
  EXPECT_FALSE(delegate_->IsRunning());
}

TEST_F(AppEventDelegateTest, MultipleStartsIgnoredByDelegate) {
  EXPECT_CALL(*runner_, OnStart(_)).Times(1);
  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeStart);
}

TEST_F(AppEventDelegateTest, EventsAfterStopIgnored) {
  EXPECT_CALL(*runner_, OnStart(_));
  SendEvent(kSbEventTypeStart);
  EXPECT_CALL(*runner_, OnBlur());
  EXPECT_CALL(*runner_, OnConceal());
  EXPECT_CALL(*runner_, OnFreeze());
  EXPECT_CALL(*runner_, OnStop());
  SendEvent(kSbEventTypeStop);

  EXPECT_CALL(*runner_, OnBlur()).Times(0);
  SendEvent(kSbEventTypeBlur);
}

TEST_F(AppEventDelegateTest, ImplicitPreload) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, OnStart(_));
    EXPECT_CALL(*runner_, OnReveal());
  }

  SendEvent(kSbEventTypeReveal);
}

}  // namespace cobalt
