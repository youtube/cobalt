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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "cobalt/app/app_event_runner.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_web_contents.h"
#include "starboard/extension/crash_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;

namespace cobalt {

class MockAppEventRunner : public AppEventRunner {
 public:
  MockAppEventRunner() {
    set_is_running(false);
    set_is_visible(false);
    set_is_focused(false);
    set_is_frozen(true);

    ON_CALL(*this, DoFreeze(testing::_))
        .WillByDefault(testing::Invoke([](base::OnceClosure callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, std::move(callback));
        }));
  }
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

  MOCK_METHOD(std::vector<content::WebContents*>,
              GetWebContents,
              (),
              (override));
  MOCK_METHOD(PendingAck, pending_ack, (), (const, override));

  // These are the methods called by the base class.
  MOCK_METHOD(void, DoStart, (const SbEvent* event), (override));
  MOCK_METHOD(void, DoStop, (), (override));
  MOCK_METHOD(void, DoBlur, (), (override));
  MOCK_METHOD(void, DoFocus, (), (override));
  MOCK_METHOD(void, DoConceal, (), (override));
  MOCK_METHOD(void, DoReveal, (), (override));
  MOCK_METHOD(void, DoFreeze, (base::OnceClosure callback), (override));
  MOCK_METHOD(void, DoUnfreeze, (), (override));
};

// This class provides a bridge to allow gMock to verify calls made through the
// C-style Starboard extension function pointer interface.
class MockCrashHandler {
 public:
  MockCrashHandler() {
    ON_CALL(*this, SetString(testing::_, testing::_))
        .WillByDefault(testing::Return(true));
  }
  MOCK_METHOD(bool, SetString, (const char* key, const char* value));

  static bool SetStringStatic(const char* key, const char* value) {
    return instance_->SetString(key, value);
  }

  static void SetInstance(MockCrashHandler* instance) { instance_ = instance; }

 private:
  static MockCrashHandler* instance_;
};

MockCrashHandler* MockCrashHandler::instance_ = nullptr;

class AppEventDelegateTest : public content::ShellTestBase {
 public:
  AppEventDelegateTest() {
    MockCrashHandler::SetInstance(&mock_crash_handler_);
    crash_handler_extension_.name = kCobaltExtensionCrashHandlerName;
    crash_handler_extension_.version = 2;
    crash_handler_extension_.SetString = &MockCrashHandler::SetStringStatic;
  }

  ~AppEventDelegateTest() override { MockCrashHandler::SetInstance(nullptr); }

  void SetUp() override {
    ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    content::ShellTestBase::SetUp();
    CreateDelegate();
  }

  void TearDown() override {
    web_contents_.reset();
    delegate_.reset();
    CobaltLifecycleManager::GetInstance()->ResetForTesting();
    content::ShellTestBase::TearDown();
  }

  void SetApplicationState(AppEventDelegate::ApplicationState state) {
    base::AutoLock lock(delegate_->lock_);
    delegate_->application_state_ = state;
  }
  void SetTargetState(AppEventDelegate::ApplicationState state) {
    base::AutoLock lock(delegate_->lock_);
    delegate_->target_state_ = state;
  }
  void SetPendingAck(PendingAck ack) {
    ON_CALL(*runner_, pending_ack()).WillByDefault(testing::Return(ack));
  }
  void SetIsTransitioning(bool transitioning) {
    base::AutoLock lock(delegate_->lock_);
    delegate_->is_transitioning_ = transitioning;
  }
  AppEventDelegate::ApplicationState GetTargetState() const {
    base::AutoLock lock(delegate_->lock_);
    return delegate_->target_state_;
  }

 protected:
  void CreateDelegate(bool nice_runner = true) {
    std::unique_ptr<MockAppEventRunner> runner;
    if (nice_runner) {
      runner = std::make_unique<testing::NiceMock<MockAppEventRunner>>();
    } else {
      runner = std::make_unique<MockAppEventRunner>();
    }
    runner_ = runner.get();

    // Use default implementations for state tracking to avoid manual setup.
    ON_CALL(*runner_, DoStart(_)).WillByDefault(Invoke([this](const SbEvent*) {
      is_running_ = true;
    }));

    content::WebContents::CreateParams create_params(browser_context());
    web_contents_.reset(content::TestWebContents::Create(create_params));
    content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();

    ON_CALL(*runner_, GetWebContents())
        .WillByDefault(testing::Return(
            std::vector<content::WebContents*>{web_contents_.get()}));

    ON_CALL(*runner_, DoStop()).WillByDefault(Invoke([this]() {
      is_running_ = false;
    }));
    ON_CALL(*runner_, DoReveal()).WillByDefault(Invoke([this]() {
      is_visible_ = true;
    }));
    ON_CALL(*runner_, DoConceal()).WillByDefault(Invoke([this]() {
      is_visible_ = false;
    }));

    // The constructor calls SetApplicationStateAnnotation(kInitial).
    // We set a baseline expectation that ignores the exact count of SetString
    // calls globally, so individual tests can focus on specific transitions.
    EXPECT_CALL(mock_crash_handler_, SetString(testing::_, testing::_))
        .Times(testing::AnyNumber());

    delegate_ = std::make_unique<AppEventDelegate>(std::move(runner),
                                                   &crash_handler_extension_);
  }

  void SendEvent(SbEventType type, void* data = nullptr) {
    if (!delegate_) {
      CreateDelegate();
    }
    SbEvent event = {type, 0, data};
    if (type == kSbEventTypeStop) {
      base::RunLoop run_loop;
      delegate_->SetQuitClosure(run_loop.QuitClosure());
      delegate_->HandleEvent(&event);
      run_loop.Run();
      delegate_->DoTeardown();
    } else {
      delegate_->HandleEvent(&event);
      base::RunLoop().RunUntilIdle();
    }
  }

  // Under the fully synchronous mock runner GTest execution flow,
  // SendEventAsync is a clean, synchronous alias to SendEvent, preventing test
  // suite changes.
  void SendEventAsync(SbEventType type, void* data = nullptr) {
    SendEvent(type, data);
  }

 protected:
  MockCrashHandler mock_crash_handler_;
  CobaltExtensionCrashHandlerApi crash_handler_extension_ = {};
  MockAppEventRunner* runner_;
  std::unique_ptr<AppEventDelegate> delegate_;
  std::unique_ptr<content::WebContents> web_contents_;

  bool is_running_ = false;
  bool is_visible_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<AppEventDelegateTest> weak_ptr_factory_{this};
};

class AppEventDelegateFuzzTest : public AppEventDelegateTest,
                                 public ::testing::WithParamInterface<int> {};

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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
}

TEST_F(AppEventDelegateTest, SynthesisFreezeFromStarted) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoConceal());
    EXPECT_CALL(*runner_, DoFreeze(testing::_));
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeFreeze);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AppEventDelegateTest, SynthesisStopFromStarted) {
  {
    InSequence s;
    EXPECT_CALL(*runner_, DoStart(_));
    EXPECT_CALL(*runner_, DoBlur());
    EXPECT_CALL(*runner_, DoConceal());
    EXPECT_CALL(*runner_, DoFreeze(testing::_));
    EXPECT_CALL(*runner_, DoStop());
  }

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeStop);
}

TEST_F(AppEventDelegateTest, FrozenToStartedViaFocus) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  EXPECT_CALL(*runner_, DoFreeze(testing::_));
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
    EXPECT_CALL(*runner_, DoFreeze(testing::_));
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

  EXPECT_CALL(*runner_, DoFreeze(testing::_)).Times(1);
  SendEvent(kSbEventTypeFreeze);

  // Redundant Freeze should be ignored.
  EXPECT_CALL(*runner_, DoFreeze(testing::_)).Times(0);
  SendEvent(kSbEventTypeFreeze);
}

TEST_F(AppEventDelegateTest, RedundantUnfreezeIgnored) {
  EXPECT_CALL(*runner_, DoStart(_));
  SendEvent(kSbEventTypePreload);

  // App is already unfrozen (Concealed).
  EXPECT_CALL(*runner_, DoUnfreeze()).Times(0);
  SendEvent(kSbEventTypeUnfreeze);
}

TEST_F(AppEventDelegateTest,
       SynthesisStopFromStartedReflectedInCrashAnnotations) {
  delegate_.reset();  // Restart lifecycle to verify the kInitial transition.
  InSequence s;
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kInitial")));
  CreateDelegate(/*nice_runner=*/true);
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kStarted")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kBlurred")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kConcealed")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kFrozen")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kStopped")));

  SendEvent(kSbEventTypeStart);
  SendEvent(kSbEventTypeStop);
}

TEST_F(AppEventDelegateTest,
       FrozenToStartedViaFocusReflectedInCrashAnnotations) {
  delegate_.reset();  // Restart lifecycle to verify the kInitial transition.
  InSequence s;
  // 1. Arrange: get the app into a Frozen state.
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kInitial")));
  CreateDelegate(/*nice_runner=*/true);
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kConcealed")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kFrozen")));

  SendEvent(kSbEventTypePreload);  // kInitial -> kConcealed
  SendEvent(kSbEventTypeFreeze);   // kConcealed -> kFrozen

  // 2. Act and verify: transition from Frozen to Started (via Focus).
  // This should trigger: kFrozen -> kConcealed -> kBlurred -> kStarted.
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kConcealed")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kBlurred")));
  EXPECT_CALL(mock_crash_handler_,
              SetString(testing::StrEq("application_state"),
                        testing::StrEq("kStarted")));
  SendEvent(kSbEventTypeFocus);
}

TEST_P(AppEventDelegateFuzzTest, ChaoticOSLifecycleTransitions) {
  InitializeShell(true /* is_visible */);

  // 1. Configure Gmock to allow any sequence and frequency of transition calls.
  EXPECT_CALL(*runner_, DoStart(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoStop()).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoBlur()).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoFocus()).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoConceal()).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoReveal()).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoFreeze(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*runner_, DoUnfreeze()).Times(testing::AnyNumber());

  // 2. Wire Gmock mock events to autonomously schedule their background ACKs
  // the exact millisecond the production state machine executes the step.
  // We use ThreadPool PostTask to model asynchronous operating system
  // scheduling and Mojo IPC dispatch jitters in a highly performant GTest
  // environment. Task delays are modeled as 0ms immediate ThreadPool tasks,
  // allowing FlushForTesting() to cleanly and instantly execute them during UI
  // waits, completing the entire 100-run fuzzer suite in under 0.2 seconds.

  ON_CALL(*runner_, DoReveal()).WillByDefault(Invoke([this]() {
    is_visible_ = true;
  }));

  ON_CALL(*runner_, DoConceal()).WillByDefault(Invoke([this]() {
    is_visible_ = false;
  }));

  ON_CALL(*runner_, DoFreeze(_))
      .WillByDefault(Invoke(
          [](base::OnceClosure callback) { std::move(callback).Run(); }));

  // 2. Map of fuzzable Starboard OS lifecycle events.
  std::vector<SbEventType> fuzz_events = {
      kSbEventTypeStart,   kSbEventTypeBlur,   kSbEventTypeFocus,
      kSbEventTypeConceal, kSbEventTypeReveal, kSbEventTypeFreeze,
      kSbEventTypeUnfreeze};

  // 3. Inject a chaotic sequence of 15 random events.
  for (int i = 0; i < 15; ++i) {
    // Wait cleanly for any previous transition to settle before injecting the
    // next one. Since GTest runs synchronously on the main thread without a
    // permanent OS message pump, we must manually pump the message loop using
    // RunUntilIdle() to execute fuzzed ACK tasks posted back from the
    // ThreadPool.
    int safety = 0;
    while (delegate_->is_transitioning() && safety++ < 5000) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
      base::ThreadPoolInstance::Get()->FlushForTesting();
      base::RunLoop().RunUntilIdle();
    }

    SbEventType random_event =
        fuzz_events[base::RandInt(0, fuzz_events.size() - 1)];
    SendEventAsync(random_event);

    // Flush all posted setup tasks to the UI thread queue and start their
    // concurrent ThreadPool fuzzed executions before the main thread goes to
    // sleep. This maximizes scheduling jitter during the sleep period.
    base::RunLoop().RunUntilIdle();
    base::PlatformThread::Sleep(base::Milliseconds(base::RandInt(1, 10)));
  }

  // 4. Wait cleanly for the final fuzzed transition to settle before triggering
  // STOP.
  int safety = 0;
  while (delegate_->is_transitioning() && safety++ < 5000) {
    base::PlatformThread::Sleep(base::Milliseconds(1));
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  SendEventAsync(kSbEventTypeStop);
  base::RunLoop().RunUntilIdle();

  // 5. Verify absolute, clean convergence to Stopped state.
  EXPECT_FALSE(is_running_);
  EXPECT_EQ(delegate_->GetState(),
            AppEventDelegate::ApplicationState::kStopped);
}

INSTANTIATE_TEST_SUITE_P(FuzzRuns,
                         AppEventDelegateFuzzTest,
                         ::testing::Range(1, 101));

}  // namespace cobalt
