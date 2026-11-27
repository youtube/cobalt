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

#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "gin/converter.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-script.h"

namespace cobalt {
namespace {

class RendererThreadParkerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(thread_.Start());
    base::WaitableEvent registered;
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          RegisterCurrentThreadAsRendererForParkingForTesting();
          registered.Signal();
        }));
    registered.Wait();
  }

  void TearDown() override {
    LetThreadContinue();
    UnparkRendererThreadForTesting();
    thread_.Stop();
  }

  // Puts the thread in the middle of a task until LetThreadContinue().
  void MakeThreadBusy() {
    thread_.task_runner()->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([this] {
                                      thread_is_busy_.Signal();
                                      thread_may_continue_.Wait();
                                    }));
    thread_is_busy_.Wait();
  }

  void LetThreadContinue() { thread_may_continue_.Signal(); }

  // Leaves a task waiting for the thread. A parked thread never runs it.
  void QueueTask() {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([this] { queued_task_ran_.store(true); }));
  }

  base::Thread thread_{"ParkTest"};
  base::WaitableEvent thread_is_busy_;
  base::WaitableEvent thread_may_continue_;
  std::atomic<bool> queued_task_ran_{false};
};

// Verifies that renderer tasks keep running while no park is requested, since
// the observer runs before every task during normal use.
TEST_F(RendererThreadParkerTest, DoesNotParkBeforeRequest) {
  base::WaitableEvent done;
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] { done.Signal(); }));
  done.Wait();
  EXPECT_FALSE(WaitUntilRendererThreadParked(TestTimeouts::tiny_timeout()));
}

// Verifies that a renderer thread with no tasks queued is woken and parked, so
// it cannot run a later task after teardown frees objects.
TEST_F(RendererThreadParkerTest, ParksIdleThread) {
  RequestRendererThreadPark();
  EXPECT_TRUE(WaitUntilRendererThreadParked(TestTimeouts::action_timeout()));

  QueueTask();
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(queued_task_ran_.load());
}

// Verifies that the renderer thread parks before tasks queued behind the
// running one, which could outlast teardown's wait and use freed objects.
TEST_F(RendererThreadParkerTest, ParksBeforeRunningQueuedTasks) {
  MakeThreadBusy();
  QueueTask();
  RequestRendererThreadPark();
  LetThreadContinue();

  EXPECT_TRUE(WaitUntilRendererThreadParked(TestTimeouts::action_timeout()));
  EXPECT_FALSE(queued_task_ran_.load());
}

// Verifies that the wait times out while one task keeps running, so shutdown is
// not blocked, and that the thread parks once that task ends.
TEST_F(RendererThreadParkerTest, WaitFailsUntilRunningTaskFinishes) {
  MakeThreadBusy();
  RequestRendererThreadPark();
  EXPECT_FALSE(WaitUntilRendererThreadParked(TestTimeouts::tiny_timeout()));

  LetThreadContinue();
  EXPECT_TRUE(WaitUntilRendererThreadParked(TestTimeouts::action_timeout()));
}

using RendererThreadParkerScriptTest = gin::V8Test;

// Verifies that the V8 interrupt parks a renderer thread inside long-running
// JavaScript, where no task starts for the observer to act on.
TEST_F(RendererThreadParkerScriptTest, ParksThreadRunningScript) {
  RegisterCurrentThreadAsRendererForParkingForTesting();
  v8::Isolate* isolate = instance_->isolate();

  bool parked = false;
  base::Thread requester("ParkRequester");
  ASSERT_TRUE(requester.Start());
  requester.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        RequestRendererThreadPark();
        parked = WaitUntilRendererThreadParked(TestTimeouts::action_timeout());
        UnparkRendererThreadForTesting();
        isolate->TerminateExecution();
      }));

  // No task can start on this thread while the script runs, so only the V8
  // interrupt can park it.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script;
  ASSERT_TRUE(
      v8::Script::Compile(context, gin::StringToV8(isolate, "for (;;) {}"))
          .ToLocal(&script));
  EXPECT_TRUE(script->Run(context).IsEmpty());
  isolate->CancelTerminateExecution();

  requester.Stop();
  EXPECT_TRUE(parked);
}

// Verifies that teardown does not wait when no renderer thread has registered,
// since there is nothing to park.
TEST(RendererThreadParkerNoThreadTest, WaitReturnsTrueWithoutRendererThread) {
  UnparkRendererThreadForTesting();
  RequestRendererThreadPark();
  EXPECT_TRUE(WaitUntilRendererThreadParked(TestTimeouts::tiny_timeout()));
  UnparkRendererThreadForTesting();
}

// Verifies that a thread registering after the park request parks on
// registering, so a stop during startup does not leave the renderer running.
TEST(RendererThreadParkerLateRegisterTest, ParksWhenRegisteredAfterRequest) {
  UnparkRendererThreadForTesting();
  RequestRendererThreadPark();

  std::atomic<bool> register_returned{false};
  base::Thread thread("LateRegister");
  ASSERT_TRUE(thread.Start());
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        RegisterCurrentThreadAsRendererForParkingForTesting();
        register_returned.store(true);
      }));

  // Give the thread time to register, since the wait returns at once if no
  // thread is registered.
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_TRUE(WaitUntilRendererThreadParked(TestTimeouts::action_timeout()));
  EXPECT_FALSE(register_returned.load());

  UnparkRendererThreadForTesting();
  thread.Stop();
}

}  // namespace
}  // namespace cobalt
