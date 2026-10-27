/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/scoped_operations_batcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ScopedOperationsBatcherTest, ExecutesTasksOnTargetThread) {
  auto target_thread = Thread::Create();
  target_thread->Start();

  bool task_executed = false;
  bool target_checked = false;

  {
    ScopedOperationsBatcher batcher(target_thread.get());
    batcher.push_back([&] {
      task_executed = true;
      target_checked = target_thread->IsCurrent();
    });
  }

  EXPECT_TRUE(task_executed);
  EXPECT_TRUE(target_checked);
}

TEST(ScopedOperationsBatcherTest, ExecutesReturnedTasksOnCallingThread) {
  // Use current thread as the calling (signaling) thread. This test runs
  // without an explicit RunLoop, because the returned tasks are executed
  // synchronously within the caller's context of `Run()` or
  // `~ScopedOperationsBatcher()`.
  auto signaling_thread = Thread::Current();

  auto target_thread = Thread::Create();
  target_thread->Start();

  bool return_task_executed = false;
  Thread* return_task_thread = nullptr;
  bool task_executed = false;
  Thread* task_thread = nullptr;

  {
    ScopedOperationsBatcher batcher(target_thread.get());
    absl::AnyInvocable<absl::AnyInvocable<void() &&>() &&> task =
        [&]() -> absl::AnyInvocable<void() &&> {
      task_executed = true;
      task_thread = Thread::Current();
      return [&]() {
        return_task_executed = true;
        return_task_thread = Thread::Current();
      };
    };
    batcher.push_back(std::move(task));
  }

  EXPECT_TRUE(task_executed);
  EXPECT_EQ(task_thread, target_thread.get());
  EXPECT_TRUE(return_task_executed);
  EXPECT_EQ(return_task_thread, signaling_thread);
}

TEST(ScopedOperationsBatcherTest, YieldsToHighPriorityTasks) {
  auto target_thread = Thread::Create();
  target_thread->Start();

  std::vector<int> execution_order;

  {
    ScopedOperationsBatcher batcher(target_thread.get());
    batcher.push_back([&] { execution_order.push_back(1); });
    batcher.push_back([&] {
      execution_order.push_back(2);
      // Post a high-priority task that should interrupt the batch.
      target_thread->PostHighPriorityTask(
          [&] { execution_order.push_back(3); });
    });
    batcher.push_back([&] { execution_order.push_back(4); });
    batcher.push_back([&] { execution_order.push_back(5); });
  }

  // Expect the high priority task (3) to execute immediately after the task
  // that posted it (2). The regular tasks (4, 5) should yield.
  EXPECT_EQ(execution_order, std::vector<int>({1, 2, 3, 4, 5}));
}

}  // namespace
}  // namespace webrtc
