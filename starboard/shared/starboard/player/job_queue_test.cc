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

#include "starboard/shared/starboard/player/job_queue.h"

#include <atomic>
#include <vector>

#include "starboard/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

using ::testing::ElementsAre;

// Require at least millisecond-level precision.
const SbTime kPrecision = kSbTimeMillisecond;

class JobQueueTest
    : public ::testing::Test,
      protected ::starboard::shared::starboard::player::JobQueue::JobOwner {
 public:
  JobQueueTest() : JobOwner(kDetached) {}
  ~JobQueueTest() {}
  // Create a JobQueue for use on the current thread.
  JobQueue job_queue_;
};

TEST_F(JobQueueTest, OwnedScheduledJobsAreExecutedInOrder) {
  AttachToCurrentThread();

  std::vector<int> values;
  Schedule([&]() { values.push_back(1); });
  Schedule([&]() { values.push_back(2); });
  Schedule([&]() { values.push_back(3); });
  Schedule([&]() { values.push_back(4); }, 1 * kPrecision);
  Schedule([&]() { values.push_back(5); }, 1 * kPrecision);
  Schedule([&]() { values.push_back(6); }, 1 * kPrecision);
  Schedule([&]() { values.push_back(7); }, 2 * kPrecision);
  Schedule([&]() { values.push_back(8); }, 3 * kPrecision);

  // Sleep past the last scheduled job.
  SbThreadSleep(4 * kPrecision);
  job_queue_.RunUntilIdle();

  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
  DetachFromCurrentThread();
}

TEST_F(JobQueueTest, OwnedJobsAreRemovedWhenOwnerGoesOutOfScope) {
  std::atomic_bool job_1 = {false}, job_2 = {false};
  {
    JobQueue::JobOwner job_owner(&job_queue_);
    job_owner.Schedule([&]() { job_1 = true; });
    job_queue_.Schedule([&]() { job_2 = true; });
  }
  // Execute pending jobs.
  job_queue_.RunUntilIdle();

  // Job 1 was canceled due to job_owner going out of scope.
  EXPECT_FALSE(job_1);
  // Job 2 should have run as it was not owned by job_owner.
  EXPECT_TRUE(job_2);
}

TEST_F(JobQueueTest, CancelPendingJobsCancelsPendingJobs) {
  AttachToCurrentThread();
  std::atomic_bool job_1 = {false}, job_2 = {false};

  Schedule([&]() { job_1 = true; });
  job_queue_.Schedule([&]() { job_2 = true; });

  // Cancel the pending owned job (job 1).
  CancelPendingJobs();

  // Execute any remaining pending jobs.
  job_queue_.RunUntilIdle();

  // Job 1 was owned by |this| and should have been canceled.
  EXPECT_FALSE(job_1);
  // Job 2 should have run.
  EXPECT_TRUE(job_2);
  DetachFromCurrentThread();
}

TEST_F(JobQueueTest, RemovedJobsAreRemoved) {
  std::atomic_bool job_1 = {false}, job_2 = {false};
  auto job_token_1 = job_queue_.Schedule([&]() { job_1 = true; });
  auto job_token_2 = job_queue_.Schedule([&]() { job_2 = true; });

  // Cancel job 1.
  job_queue_.RemoveJobByToken(job_token_1);

  job_queue_.RunUntilIdle();

  // Job 1 should have been canceled.
  EXPECT_FALSE(job_1);
  EXPECT_TRUE(job_2);

  // Should be a no-op since job 2 already ran.
  job_queue_.RemoveJobByToken(job_token_2);
}

TEST_F(JobQueueTest, RunUntilStoppedExecutesAllRemainingJobs) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  std::atomic_bool job_1 = {false}, job_2 = {false}, job_3 = {false};
  job_queue_.Schedule([&job_1]() { job_1 = true; });
  job_queue_.Schedule([&job_2]() { job_2 = true; }, 1 * kPrecision);
  job_queue_.Schedule([&job_3]() { job_3 = true; }, 2 * kPrecision);
  job_queue_.Schedule([&]() { job_queue_.StopSoon(); }, 3 * kPrecision);

  job_queue_.RunUntilIdle();
  // Job 1 should have been executed.
  EXPECT_TRUE(job_1);

  job_queue_.RunUntilStopped();

  // All other pending jobs should have run.
  EXPECT_TRUE(job_2);
  EXPECT_TRUE(job_3);

  // Time passed should at least be as long as the delay of the last job.
  EXPECT_GE(SbTimeGetMonotonicNow() - start, 3 * kPrecision);
}

TEST_F(JobQueueTest, JobsAreMovedAndNotCopied) {
  struct MoveMe {
   public:
    MoveMe() {}
    MoveMe(MoveMe const& other) {
      copied = true;
      moved = other.moved;
    }
    MoveMe(MoveMe&& other) {
      copied = other.copied;
      moved = true;
    }

    bool copied = false;
    bool moved = false;
  };

  std::atomic_bool moved = {false}, copied = {false};
  MoveMe move_me;
  auto job = [ move_me = std::move(move_me), &moved, &copied ]() {
    moved = move_me.moved;
    copied = move_me.copied;
  };
  job_queue_.Schedule(std::move(job));
  job_queue_.RunUntilIdle();

  EXPECT_FALSE(copied);
  EXPECT_TRUE(moved);
}

TEST_F(JobQueueTest, QueueBelongsToCorrectThread) {
  EXPECT_EQ(&job_queue_, JobQueue::current());
  EXPECT_TRUE(job_queue_.BelongsToCurrentThread());
}

}  // namespace
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
