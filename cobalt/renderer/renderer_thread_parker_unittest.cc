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

#include "cobalt/renderer/renderer_thread_parker.h"

#include <atomic>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class RendererThreadParkerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(thread_.Start());
    // Installed from inside a task, so the observer sees that task end without
    // having seen it start.
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RendererThreadParker::InstallOnCurrentThread,
                                  base::Unretained(&parker_)));
    thread_.FlushForTesting();
  }

  void TearDown() override {
    // Let the observed thread run to completion so that it can be joined.
    parker_.UnparkForTesting();
  }

  // Posts a task to the observed thread that records that it ran.
  void PostMarkerTask() {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this]() { ran_.store(true); }));
  }

  // Waits for the observed thread to park. Returns false if it does not.
  bool WaitUntilParked() {
    base::ElapsedTimer timer;
    while (!parker_.is_parked()) {
      if (timer.Elapsed() > TestTimeouts::action_timeout()) {
        return false;
      }
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    return true;
  }

  base::test::TaskEnvironment task_environment_;
  // Declared before |thread_| so that it outlives the thread it observes.
  RendererThreadParker parker_;
  base::Thread thread_{"ObservedThread"};
  std::atomic<bool> ran_{false};
};

TEST(RendererThreadParkerNotInstalledTest, RequestParkFails) {
  RendererThreadParker parker;
  EXPECT_FALSE(parker.RequestPark());
}

TEST_F(RendererThreadParkerTest, TasksRunWhenParkNotRequested) {
  PostMarkerTask();
  thread_.FlushForTesting();
  EXPECT_TRUE(ran_.load());
  EXPECT_FALSE(parker_.is_parked());
}

TEST_F(RendererThreadParkerTest, IdleThreadParksBeforeNextTask) {
  ASSERT_TRUE(parker_.RequestPark());
  // The thread is idle, so it is already quiescent.
  EXPECT_TRUE(parker_.WaitUntilQuiescent(base::TimeDelta()));
  EXPECT_FALSE(parker_.is_parked());

  PostMarkerTask();
  ASSERT_TRUE(WaitUntilParked());
  EXPECT_FALSE(ran_.load());

  parker_.UnparkForTesting();
  thread_.FlushForTesting();
  EXPECT_TRUE(ran_.load());
}

TEST_F(RendererThreadParkerTest, BusyThreadIsNotQuiescentUntilTaskFinishes) {
  base::WaitableEvent task_started;
  base::WaitableEvent finish_task;
  thread_.task_runner()->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    task_started.Signal();
                                    finish_task.Wait();
                                  }));
  task_started.Wait();

  ASSERT_TRUE(parker_.RequestPark());
  // The running task is not interrupted, so the thread is not quiescent yet.
  EXPECT_FALSE(parker_.WaitUntilQuiescent(base::TimeDelta()));

  finish_task.Signal();
  EXPECT_TRUE(parker_.WaitUntilQuiescent(TestTimeouts::action_timeout()));

  // The task that was running finished; the next one does not start.
  PostMarkerTask();
  ASSERT_TRUE(WaitUntilParked());
  EXPECT_FALSE(ran_.load());
}

TEST_F(RendererThreadParkerTest, ParkRequestStaysInEffectAfterTimeout) {
  base::WaitableEvent task_started;
  base::WaitableEvent finish_task;
  thread_.task_runner()->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                    task_started.Signal();
                                    finish_task.Wait();
                                  }));
  task_started.Wait();

  ASSERT_TRUE(parker_.RequestPark());
  EXPECT_FALSE(parker_.WaitUntilQuiescent(base::Milliseconds(1)));

  // The wait gave up, but the thread still parks once its task finishes.
  PostMarkerTask();
  finish_task.Signal();
  ASSERT_TRUE(WaitUntilParked());
  EXPECT_FALSE(ran_.load());
}

}  // namespace
}  // namespace cobalt
