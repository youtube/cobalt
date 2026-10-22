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

#include "cobalt/starboard_wasm/event_queue.h"

#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

WasmSbEvent Event(uint32_t player, int32_t a) {
  return WasmSbEvent{kWasmSbEventPlayerStatus, player, a, 0, 0};
}

class EventQueueTest : public testing::Test {
 protected:
  EventQueueTest()
      : queue_(base::MakeRefCounted<EventQueue>(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindRepeating([](int* wakes) { ++*wakes; }, &wakes_))) {}

  std::vector<WasmSbEvent> Poll(size_t max) {
    std::vector<WasmSbEvent> out(max);
    out.resize(queue_->Poll(base::as_writable_bytes(base::span(out))));
    return out;
  }

  base::test::TaskEnvironment task_environment_;
  int wakes_ = 0;
  scoped_refptr<EventQueue> queue_;
};

TEST_F(EventQueueTest, PollsInOrder) {
  queue_->Push(Event(1, 1));
  queue_->Push(Event(1, 2));
  const std::vector<WasmSbEvent> events = Poll(8);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].kind, kWasmSbEventPlayerStatus);
  EXPECT_EQ(events[0].player, 1u);
  EXPECT_EQ(events[0].a, 1);
  EXPECT_EQ(events[1].a, 2);
  EXPECT_TRUE(Poll(8).empty());
}

TEST_F(EventQueueTest, WakesOnlyWhenRequested) {
  queue_->Push(Event(1, 1));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 0);
}

TEST_F(EventQueueTest, WakesOncePerRequest) {
  queue_->RequestWake();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 0);

  queue_->Push(Event(1, 1));
  queue_->Push(Event(1, 2));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 1);

  queue_->Push(Event(1, 3));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 1);
}

TEST_F(EventQueueTest, WakesRightAwayIfEventsAreQueued) {
  queue_->Push(Event(1, 1));
  queue_->RequestWake();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 1);
}

TEST_F(EventQueueTest, DoesNotWakeAfterShutdown) {
  queue_->RequestWake();
  queue_->Push(Event(1, 1));
  queue_->Shutdown();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(wakes_, 0);
}

TEST_F(EventQueueTest, DropsEventsOfOnePlayer) {
  queue_->Push(Event(1, 0));
  queue_->Push(Event(2, 0));
  queue_->Push(Event(1, 1));
  queue_->DropPlayer(1);
  const std::vector<WasmSbEvent> events = Poll(8);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].player, 2u);
}

TEST_F(EventQueueTest, NeverDropsOrCoalesces) {
  for (int i = 0; i < 5000; ++i) {
    queue_->Push(Event(1, i % 2));
  }
  const std::vector<WasmSbEvent> events = Poll(10000);
  ASSERT_EQ(events.size(), 5000u);
  EXPECT_EQ(events.front().a, 0);
  EXPECT_EQ(events.back().a, 1);
}

TEST_F(EventQueueTest, AcceptsPushesFromOtherThreads) {
  constexpr int kThreads = 4;
  constexpr int kEventsPerThread = 1000;
  for (int t = 0; t < kThreads; ++t) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<EventQueue> queue, uint32_t player) {
                         for (int i = 0; i < kEventsPerThread; ++i) {
                           queue->Push(Event(player, i));
                         }
                       },
                       queue_, static_cast<uint32_t>(t)));
  }
  std::vector<int> next(kThreads, 0);
  int received = 0;
  while (received < kThreads * kEventsPerThread) {
    for (const WasmSbEvent& event : Poll(64)) {
      ASSERT_LT(event.player, static_cast<uint32_t>(kThreads));
      EXPECT_EQ(event.a, next[event.player]++);
      ++received;
    }
    base::PlatformThread::YieldCurrentThread();
  }
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(Poll(8).empty());
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
