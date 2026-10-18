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

#include "cobalt/starboard_wasm/core/event_queue.h"

#include <vector>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

class EventQueueTest : public testing::Test {
 protected:
  EventQueueTest()
      : queue_(base::SequencedTaskRunner::GetCurrentDefault(),
               base::BindRepeating([](int* wakes) { ++*wakes; }, &wakes_)) {}

  std::vector<sb_event> PollAll() {
    std::vector<sb_event> out(2048);
    out.resize(queue_.Poll(reinterpret_cast<uint8_t*>(out.data()),
                           static_cast<uint32_t>(out.size())));
    return out;
  }

  base::test::TaskEnvironment task_environment_;
  int wakes_ = 0;
  EventQueue queue_;
};

TEST_F(EventQueueTest, PollsInOrder) {
  queue_.Push(1, 7, 10, 20);
  queue_.Push(2, 8, 30, 40);
  const std::vector<sb_event> events = PollAll();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].struct_size, sizeof(sb_event));
  EXPECT_EQ(events[0].type, 1u);
  EXPECT_EQ(events[0].handle, 7u);
  EXPECT_EQ(events[0].a, 10);
  EXPECT_EQ(events[0].b, 20);
  EXPECT_EQ(events[1].type, 2u);
  EXPECT_TRUE(PollAll().empty());
}

TEST_F(EventQueueTest, WakesOnceWhenQueueBecomesNonEmpty) {
  queue_.Push(1, 0, 0, 0);
  queue_.Push(2, 0, 0, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wakes_, 1);

  PollAll();
  queue_.Push(3, 0, 0, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wakes_, 2);
}

TEST_F(EventQueueTest, SkipsWakeIfDrainedFirst) {
  queue_.Push(1, 0, 0, 0);
  PollAll();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wakes_, 0);
}

TEST_F(EventQueueTest, NeverDropsEvents) {
  for (int i = 0; i < 1500; ++i) {
    queue_.Push(1, 0, i, 0);
  }
  const std::vector<sb_event> events = PollAll();
  ASSERT_EQ(events.size(), 1500u);
  EXPECT_EQ(events.front().a, 0);
  EXPECT_EQ(events.back().a, 1499);
}

TEST_F(EventQueueTest, CoalescesByTypeHandleAndA) {
  queue_.PushCoalesced(1, 7, 0, 1);
  queue_.Push(2, 7, 3, 1);
  queue_.PushCoalesced(1, 7, 1, 1);
  queue_.PushCoalesced(1, 8, 0, 1);
  queue_.PushCoalesced(1, 7, 0, 2);
  const std::vector<sb_event> events = PollAll();
  ASSERT_EQ(events.size(), 4u);
  EXPECT_EQ(events[0].type, 2u);
  EXPECT_EQ(events[1].handle, 7u);
  EXPECT_EQ(events[1].a, 1);
  EXPECT_EQ(events[2].handle, 8u);
  EXPECT_EQ(events[3].handle, 7u);
  EXPECT_EQ(events[3].a, 0);
  EXPECT_EQ(events[3].b, 2);
}

TEST_F(EventQueueTest, CoalescingKeepsOtherEvents) {
  for (int i = 0; i < 1500; ++i) {
    queue_.PushCoalesced(1, 7, 0, i);
    queue_.Push(2, 7, i, 0);
  }
  const std::vector<sb_event> events = PollAll();
  ASSERT_EQ(events.size(), 1501u);
  EXPECT_EQ(events.front().type, 2u);
  EXPECT_EQ(events.front().a, 0);
  EXPECT_EQ(events[events.size() - 2].type, 1u);
  EXPECT_EQ(events[events.size() - 2].b, 1499);
  EXPECT_EQ(events.back().type, 2u);
  EXPECT_EQ(events.back().a, 1499);
}

TEST_F(EventQueueTest, CoalescedPushWakes) {
  queue_.PushCoalesced(1, 0, 0, 0);
  queue_.PushCoalesced(1, 0, 0, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wakes_, 1);
  EXPECT_EQ(PollAll().size(), 1u);
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
