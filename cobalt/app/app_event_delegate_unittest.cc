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

#include "cobalt/app/app_event_runner.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;

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
  MOCK_METHOD(void, OnInput, (const SbEvent* event), (override));
  MOCK_METHOD(void, OnLink, (const SbEvent* event), (override));
  MOCK_METHOD(void, OnLowMemory, (const SbEvent* event), (override));
  MOCK_METHOD(void, OnWindowSizeChanged, (const SbEvent* event), (override));
  MOCK_METHOD(void,
              OnOsNetworkConnectedDisconnected,
              (const SbEvent* event),
              (override));
  MOCK_METHOD(void,
              OnDateTimeConfigurationChanged,
              (const SbEvent* event),
              (override));
  MOCK_METHOD(void,
              OnAccessibilityTextToSpeechSettingsChanged,
              (const SbEvent* event),
              (override));

  // These are the methods called by the base class.
  MOCK_METHOD(void, DoStart, (const SbEvent* event), (override));
  MOCK_METHOD(void, DoStop, (), (override));
  MOCK_METHOD(void, DoBlur, (), (override));
  MOCK_METHOD(void, DoFocus, (), (override));
  MOCK_METHOD(void, DoConceal, (), (override));
  MOCK_METHOD(void, DoReveal, (), (override));
  MOCK_METHOD(void, DoFreeze, (), (override));
  MOCK_METHOD(void, DoUnfreeze, (), (override));
};

class AppEventDelegateTest : public ::testing::Test {
 public:
  AppEventDelegateTest() {
    auto runner = std::make_unique<MockAppEventRunner>();
    runner_ = runner.get();

    // Use default implementations for state tracking to avoid manual setup.
    ON_CALL(*runner_, DoStart(_)).WillByDefault(Invoke([this](const SbEvent*) {
      is_running_ = true;
    }));
    ON_CALL(*runner_, DoStop()).WillByDefault(Invoke([this]() {
      is_running_ = false;
    }));
    ON_CALL(*runner_, DoReveal()).WillByDefault(Invoke([this]() {
      is_visible_ = true;
    }));
    ON_CALL(*runner_, DoConceal()).WillByDefault(Invoke([this]() {
      is_visible_ = false;
    }));

    delegate_ = std::make_unique<AppEventDelegate>(std::move(runner));
  }

 protected:
  void SendEvent(SbEventType type, void* data = nullptr) {
    SbEvent event = {type, 0, data};
    delegate_->HandleEvent(&event);
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  MockAppEventRunner* runner_;
  std::unique_ptr<AppEventDelegate> delegate_;

  bool is_running_ = false;
  bool is_visible_ = false;
};

TEST_F(AppEventDelegateTest, StartVisible) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kStarted);
}

TEST_F(AppEventDelegateTest, StartPreloaded) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kConcealed);
}

TEST_F(AppEventDelegateTest, LinearityBlur) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeBlur);
}

TEST_F(AppEventDelegateTest, SynthesisFocusFromStopped) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));  // Implicit preload
    EXPECT_CALL(*runner_, DoReveal());
    EXPECT_CALL(*runner_, DoFocus());
  }

  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, SynthesisFocusFromPreload) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoReveal());
    EXPECT_CALL(*runner_, DoFocus());
  }

  SendEvent(kSbEventTypePreload);
  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, SynthesisFreezeFromStarted) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoConceal());
    EXPECT_CALL(*runner_, DoFreeze());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeFreeze);
}

TEST_F(AppEventDelegateTest, SynthesisStopFromStarted) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoConceal());
    EXPECT_CALL(*runner_, DoFreeze());
    EXPECT_CALL(*runner_, DoStop());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeStop);
}

TEST_F(AppEventDelegateTest, FrozenToStartedViaFocus) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  EXPECT_CALL(*runner_, DoFreeze());
  SendEvent(kSbEventTypeFreeze);

  {
    InSequence s;
    EXPECT_CALL(*runner_, DoUnfreeze());
    EXPECT_CALL(*runner_, DoReveal());
    EXPECT_CALL(*runner_, DoFocus());
  }

  SendEvent(kSbEventTypeFocus);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kStarted);
}

TEST_F(AppEventDelegateTest, NonLifecycleEventRouting) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  EXPECT_CALL(*runner_, OnInput(_));
  SendEvent(kSbEventTypeInput);
}

TEST_F(AppEventDelegateTest, RapidInterruptedSequence) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoFocus());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeBlur);
  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, MultipleStartsIgnoredByDelegate) {
  EXPECT_CALL(*runner_, DoStart(_)).Times(1);
  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeStart);
}

TEST_F(AppEventDelegateTest, RedundantStartMovesToStarted) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  // Transition to Blurred.
  EXPECT_CALL(*runner_, DoBlur());
  SendEvent(kSbEventTypeBlur);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kBlurred);

  // Redundant Start should move back to Started (Focus).
  EXPECT_CALL(*runner_, DoFocus());
  SendEvent(kSbEventTypeStart);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kStarted);
}

TEST_F(AppEventDelegateTest, RedundantStartWithLink) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  const char* kLink = "https://example.com";
  SbEventStartData data = {nullptr, 0, kLink};
  EXPECT_CALL(*runner_, OnLink(_))
      .WillOnce(Invoke([kLink](const SbEvent* event) {
        EXPECT_STREQ(static_cast<const char*>(event->data), kLink);
      }));

  SendEvent(kSbEventTypeStart, &data);
}

TEST_F(AppEventDelegateTest, EventsAfterStopIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  {
    InSequence s;
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoConceal());
    EXPECT_CALL(*runner_, DoFreeze());
    EXPECT_CALL(*runner_, DoStop());
  }

  SendEvent(kSbEventTypeStop);

  EXPECT_CALL(*runner_, DoBlur()).Times(0);

  SendEvent(kSbEventTypeBlur);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kStopped);
  EXPECT_FALSE(delegate_->IsRunning());
}

TEST_F(AppEventDelegateTest, ImplicitPreload) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoReveal());
  }

  SendEvent(kSbEventTypeReveal);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kBlurred);
  EXPECT_TRUE(delegate_->IsRunning());
  EXPECT_TRUE(delegate_->IsVisible());
  EXPECT_FALSE(delegate_->IsFocused());
}

TEST_F(AppEventDelegateTest, RedundantRevealIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  // App is already visible (Started).
  EXPECT_CALL(*runner_, DoReveal()).Times(0);
  SendEvent(kSbEventTypeReveal);
}

TEST_F(AppEventDelegateTest, RedundantConcealIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  // App is already concealed (Concealed).
  EXPECT_CALL(*runner_, DoConceal()).Times(0);
  SendEvent(kSbEventTypeConceal);
}

TEST_F(AppEventDelegateTest, RedundantBlurIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  EXPECT_CALL(*runner_, DoBlur()).Times(1);
  SendEvent(kSbEventTypeBlur);

  // Redundant Blur should be ignored.
  EXPECT_CALL(*runner_, DoBlur()).Times(0);
  SendEvent(kSbEventTypeBlur);
}

TEST_F(AppEventDelegateTest, RedundantFocusIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypeStart);

  // App is already focused (Started).
  EXPECT_CALL(*runner_, DoFocus()).Times(0);
  SendEvent(kSbEventTypeFocus);
}

TEST_F(AppEventDelegateTest, RedundantFreezeIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  EXPECT_CALL(*runner_, DoFreeze()).Times(1);
  SendEvent(kSbEventTypeFreeze);

  // Redundant Freeze should be ignored.
  EXPECT_CALL(*runner_, DoFreeze()).Times(0);
  SendEvent(kSbEventTypeFreeze);
}

TEST_F(AppEventDelegateTest, RedundantUnfreezeIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  // App is already unfrozen (Concealed).
  EXPECT_CALL(*runner_, DoUnfreeze()).Times(0);
  SendEvent(kSbEventTypeUnfreeze);
}

}  // namespace cobalt
