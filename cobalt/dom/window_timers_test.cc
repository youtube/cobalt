// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/window_timers.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "net/test/test_with_scoped_task_environment.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

namespace testing {

using ::testing::_;
using ::testing::Return;

class MockTimerCallback : public WindowTimers::TimerCallback {
 public:
  MOCK_CONST_METHOD0(Run, script::CallbackResult<void>());
  void ExpectRunCall(int times) {
    EXPECT_CALL(*this, Run())
        .Times(times)
        .WillRepeatedly(Return(script::CallbackResult<void>()));
  }
};

class MockDebuggerHooks : public base::DebuggerHooks {
 public:
  MOCK_CONST_METHOD2(ConsoleLog, void(::logging::LogSeverity, std::string));
  MOCK_CONST_METHOD3(AsyncTaskScheduled,
                     void(const void*, const std::string&, AsyncTaskFrequency));
  MOCK_CONST_METHOD1(AsyncTaskStarted, void(const void*));
  MOCK_CONST_METHOD1(AsyncTaskFinished, void(const void*));
  MOCK_CONST_METHOD1(AsyncTaskCanceled, void(const void*));

  void ExpectAsyncTaskScheduled(int times) {
    EXPECT_CALL(*this, AsyncTaskScheduled(_, _, _)).Times(times);
  }
  void ExpectAsyncTaskStarted(int times) {
    EXPECT_CALL(*this, AsyncTaskStarted(_)).Times(times);
  }
  void ExpectAsyncTaskFinished(int times) {
    EXPECT_CALL(*this, AsyncTaskFinished(_)).Times(times);
  }
  void ExpectAsyncTaskCanceled(int times) {
    EXPECT_CALL(*this, AsyncTaskCanceled(_)).Times(times);
  }
};

}  // namespace testing

namespace {
const int kTimerDelayInMilliseconds = 100;
}  // namespace

using ::cobalt::script::testing::FakeScriptValue;

class WindowTimersTest : public ::testing::Test,
                         public net::WithScopedTaskEnvironment {
 protected:
  WindowTimersTest()
      : WithScopedTaskEnvironment(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        dom_stat_tracker_("WindowTimersTest"),
        callback_(&mock_timer_callback_) {
    script::Wrappable* foo = nullptr;
    timers_.reset(
        new WindowTimers(foo, &dom_stat_tracker_, hooks_,
                         base::ApplicationState::kApplicationStateStarted));
  }

  ~WindowTimersTest() override {}

  testing::MockDebuggerHooks hooks_;
  DomStatTracker dom_stat_tracker_;
  std::unique_ptr<WindowTimers> timers_;
  testing::MockTimerCallback mock_timer_callback_;
  FakeScriptValue<WindowTimers::TimerCallback> callback_;
};

TEST_F(WindowTimersTest, TimeoutIsNotCalledDirectly) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);

  mock_timer_callback_.ExpectRunCall(0);
  timers_->SetTimeout(callback_, 0);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(0));
}

TEST_F(WindowTimersTest, TimeoutZeroIsCalledImmediatelyFromTask) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(1);
  hooks_.ExpectAsyncTaskFinished(1);

  mock_timer_callback_.ExpectRunCall(1);
  timers_->SetTimeout(callback_, 0);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(0));

  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);
}

TEST_F(WindowTimersTest, TimeoutIsNotCalledBeforeDelay) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);

  hooks_.ExpectAsyncTaskStarted(0);
  hooks_.ExpectAsyncTaskFinished(0);
  mock_timer_callback_.ExpectRunCall(0);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds - 1));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
}

TEST_F(WindowTimersTest, TimeoutIsCalledAfterDelay) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(1);
  hooks_.ExpectAsyncTaskFinished(1);

  mock_timer_callback_.ExpectRunCall(1);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);
}

TEST_F(WindowTimersTest, TimeoutIsNotCalledRepeatedly) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(1);
  hooks_.ExpectAsyncTaskFinished(1);

  mock_timer_callback_.ExpectRunCall(1);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(10 * kTimerDelayInMilliseconds));
  FastForwardUntilNoTasksRemain();
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);
}

TEST_F(WindowTimersTest, TimeoutIsCalledWhenDelayed) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(1);
  hooks_.ExpectAsyncTaskFinished(1);

  mock_timer_callback_.ExpectRunCall(1);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds + 1000));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);
}

TEST_F(WindowTimersTest, MultipleTimeouts) {
  hooks_.ExpectAsyncTaskScheduled(2);
  hooks_.ExpectAsyncTaskCanceled(2);
  hooks_.ExpectAsyncTaskStarted(2);
  hooks_.ExpectAsyncTaskFinished(2);

  mock_timer_callback_.ExpectRunCall(2);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds * 3);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(3 * kTimerDelayInMilliseconds));
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);
}

TEST_F(WindowTimersTest, ActiveTimeoutsAreCounted) {
  hooks_.ExpectAsyncTaskScheduled(2);
  hooks_.ExpectAsyncTaskCanceled(2);
  hooks_.ExpectAsyncTaskStarted(2);
  hooks_.ExpectAsyncTaskFinished(2);

  mock_timer_callback_.ExpectRunCall(2);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds * 3);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(3 * kTimerDelayInMilliseconds));
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));
}

TEST_F(WindowTimersTest, IntervalZeroTaskIsScheduledImmediately) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);

  timers_->SetInterval(callback_, 0);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(0));

  // Note: We can't use RunUntilIdle() or FastForwardBy() in this case,
  // because the task queue never gets idle.
}

TEST_F(WindowTimersTest, IntervalIsNotCalledBeforeDelay) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);

  hooks_.ExpectAsyncTaskStarted(0);
  hooks_.ExpectAsyncTaskFinished(0);
  mock_timer_callback_.ExpectRunCall(0);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds - 1));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
}

TEST_F(WindowTimersTest, IntervalIsCalledAfterDelay) {
  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(1);
  hooks_.ExpectAsyncTaskFinished(1);

  mock_timer_callback_.ExpectRunCall(1);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
}

TEST_F(WindowTimersTest, IntervalIsCalledRepeatedly) {
  int interval_count = 10;

  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(interval_count);
  hooks_.ExpectAsyncTaskFinished(interval_count);

  mock_timer_callback_.ExpectRunCall(interval_count);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(base::TimeDelta::FromMilliseconds(interval_count *
                                                  kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
}

TEST_F(WindowTimersTest, IntervalDrifts) {
  int interval_count = 10;

  hooks_.ExpectAsyncTaskScheduled(1);
  hooks_.ExpectAsyncTaskCanceled(1);
  hooks_.ExpectAsyncTaskStarted(interval_count);
  hooks_.ExpectAsyncTaskFinished(interval_count);

  mock_timer_callback_.ExpectRunCall(interval_count);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  while (interval_count--) {
    AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds + 1000));
    RunUntilIdle();
  }
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 1);
}

TEST_F(WindowTimersTest, MultipleIntervals) {
  hooks_.ExpectAsyncTaskScheduled(2);
  hooks_.ExpectAsyncTaskCanceled(2);
  hooks_.ExpectAsyncTaskStarted(4);
  hooks_.ExpectAsyncTaskFinished(4);

  mock_timer_callback_.ExpectRunCall(4);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds * 3);

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(3 * kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);
}

TEST_F(WindowTimersTest, ActiveIntervalsAreCounted) {
  hooks_.ExpectAsyncTaskScheduled(2);
  hooks_.ExpectAsyncTaskCanceled(2);
  hooks_.ExpectAsyncTaskStarted(4);
  hooks_.ExpectAsyncTaskFinished(4);

  mock_timer_callback_.ExpectRunCall(4);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds * 3);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(3 * kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));
}

TEST_F(WindowTimersTest, ActiveIntervalsAndTimeoutsAreCounted) {
  hooks_.ExpectAsyncTaskScheduled(4);
  hooks_.ExpectAsyncTaskCanceled(4);
  hooks_.ExpectAsyncTaskStarted(6);
  hooks_.ExpectAsyncTaskFinished(6);

  mock_timer_callback_.ExpectRunCall(6);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds);
  timers_->SetInterval(callback_, kTimerDelayInMilliseconds * 3);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds);
  timers_->SetTimeout(callback_, kTimerDelayInMilliseconds * 3);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));

  EXPECT_EQ(GetPendingMainThreadTaskCount(), 4);
  EXPECT_EQ(NextMainThreadPendingTaskDelay(),
            base::TimeDelta::FromMilliseconds(kTimerDelayInMilliseconds));

  FastForwardBy(
      base::TimeDelta::FromMilliseconds(3 * kTimerDelayInMilliseconds));
  RunUntilIdle();
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 2);

  dom_stat_tracker_.FlushPeriodicTracking();
  EXPECT_EQ("2", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Interval")
                     .value_or("Foo"));
  EXPECT_EQ("0", base::CValManager::GetInstance()
                     ->GetValueAsString(
                         "Count.WindowTimersTest.DOM.WindowTimers.Timeout")
                     .value_or("Foo"));
}


}  // namespace dom
}  // namespace cobalt
