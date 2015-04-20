// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/test_task_runner.h"

#include <vector>

#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

PostedTask::PostedTask(const tracked_objects::Location& location,
                       const base::Closure& closure,
                       base::TimeDelta delta,
                       base::TimeTicks time)
    : location(location),
      closure(closure),
      delta(delta),
      time(time) {
}

PostedTask::~PostedTask() {
}

TestTaskRunner::TestTaskRunner(MockClock* clock)
    : clock_(clock) {
}

TestTaskRunner::~TestTaskRunner() {
}

bool TestTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

bool TestTaskRunner::PostDelayedTask(const tracked_objects::Location& location,
                                     const base::Closure& closure,
                                     base::TimeDelta delta) {
  EXPECT_GE(delta, base::TimeDelta());
  tasks_.push_back(PostedTask(location, closure, delta,
                              clock_->NowInTicks() + delta));
  return false;
}

PostedTask TestTaskRunner::GetTask(size_t n) {
  return tasks_.at(n);
}

std::vector<PostedTask>::iterator TestTaskRunner::FindNextTask() {
  if (tasks_.size() == 0) {
    return tasks_.end();
  } else {
    std::vector<PostedTask>::iterator next = tasks_.begin();
    for (std::vector<PostedTask>::iterator it = next + 1; it != tasks_.end();
         ++it) {
      // Note, that this gives preference to FIFO when times match.
      if (it->time < next->time) {
        next = it;
      }
    }
    return next;
  }
}

void TestTaskRunner::RunNextTask() {
  // Find the next task to run, advance the time to the correct time
  // and then run the task.
  std::vector<PostedTask>::iterator next = FindNextTask();
  DCHECK(next != tasks_.end());
  clock_->AdvanceTime(QuicTime::Delta::FromMicroseconds(
      (next->time - clock_->NowInTicks()).InMicroseconds()));
  PostedTask task = *next;
  tasks_.erase(next);
  task.closure.Run();
}

}  // namespace test
}  // namespace net
