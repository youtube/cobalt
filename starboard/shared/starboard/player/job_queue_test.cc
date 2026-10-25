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

#include <unistd.h>

#include <atomic>
#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

namespace {

using ::testing::ElementsAre;

// Require at least millisecond-level precision.
const int64_t kPrecisionUsec = 1000;

// The tests are run with and without optimizations, the parameter is whether
// they are enabled.
class JobQueueTest : public ::testing::TestWithParam<bool> {
 public:
  JobQueueTest() : scoped_optimizations_(GetParam()), job_owner_(&job_queue_) {}
  ~JobQueueTest() {}

  // Declared before |job_queue_| so that the switch is set when the queue is
  // constructed.
  JobQueue::ScopedOptimizationsForTesting scoped_optimizations_;
  JobQueue job_queue_;
  JobQueue::JobOwner job_owner_;
};

TEST_P(JobQueueTest, UsesRequestedOptimizations) {
  EXPECT_EQ(job_queue_.optimizations_enabled(), GetParam());
}

TEST_P(JobQueueTest, OwnedScheduledJobsAreExecutedInOrder) {
  std::vector<int> values;
  job_owner_.Schedule([&]() { values.push_back(1); });
  job_owner_.Schedule([&]() { values.push_back(2); });
  job_owner_.Schedule([&]() { values.push_back(3); });
  job_owner_.Schedule([&]() { values.push_back(4); }, 1 * kPrecisionUsec);
  job_owner_.Schedule([&]() { values.push_back(5); }, 1 * kPrecisionUsec);
  job_owner_.Schedule([&]() { values.push_back(6); }, 1 * kPrecisionUsec);
  job_owner_.Schedule([&]() { values.push_back(7); }, 2 * kPrecisionUsec);
  job_owner_.Schedule([&]() { values.push_back(8); }, 3 * kPrecisionUsec);

  // Sleep past the last scheduled job.
  usleep(4 * kPrecisionUsec);
  job_queue_.RunUntilIdle();

  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
}

TEST_P(JobQueueTest, OwnedJobsAreRemovedWhenOwnerGoesOutOfScope) {
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

TEST_P(JobQueueTest, CancelPendingJobsCancelsPendingJobs) {
  std::atomic_bool job_1 = {false}, job_2 = {false};

  job_owner_.Schedule([&] { job_1 = true; });
  job_queue_.Schedule([&] { job_2 = true; });

  // Cancel the pending owned job (job 1).
  job_owner_.CancelPendingJobs();

  // Execute any remaining pending jobs.
  job_queue_.RunUntilIdle();

  // Job 1 was owned by |this| and should have been canceled.
  EXPECT_FALSE(job_1);
  // Job 2 should have run.
  EXPECT_TRUE(job_2);
}

TEST_P(JobQueueTest, RemovedJobsAreRemoved) {
  std::atomic_bool job_1 = {false}, job_2 = {false};
  auto job_token_1 = job_queue_.Schedule([&]() { job_1 = true; });
  auto job_token_2 = job_queue_.Schedule([&]() { job_2 = true; });

  // Cancel job 1.
  job_queue_.RemoveJobByToken(&job_token_1);

  job_queue_.RunUntilIdle();

  // Job 1 should have been canceled.
  EXPECT_FALSE(job_1);
  EXPECT_TRUE(job_2);

  // Should be a no-op since job 2 already ran.
  job_queue_.RemoveJobByToken(&job_token_2);
}

TEST_P(JobQueueTest, RunUntilStoppedExecutesAllRemainingJobs) {
  int64_t start = CurrentMonotonicTime();
  std::atomic_bool job_1 = {false}, job_2 = {false}, job_3 = {false};
  job_queue_.Schedule([&job_1]() { job_1 = true; });
  job_queue_.Schedule([&job_2]() { job_2 = true; }, 1 * kPrecisionUsec);
  job_queue_.Schedule([&job_3]() { job_3 = true; }, 2 * kPrecisionUsec);
  job_queue_.Schedule([&]() { job_queue_.StopSoon(); }, 3 * kPrecisionUsec);

  job_queue_.RunUntilIdle();
  // Job 1 should have been executed.
  EXPECT_TRUE(job_1);

  job_queue_.RunUntilStopped();

  // All other pending jobs should have run.
  EXPECT_TRUE(job_2);
  EXPECT_TRUE(job_3);

  // Time passed should at least be as long as the delay of the last job.
  EXPECT_GE(CurrentMonotonicTime() - start, 3 * kPrecisionUsec);
}

TEST_P(JobQueueTest, JobsAreMovedAndNotCopied) {
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
  auto job = [move_me = std::move(move_me), &moved, &copied]() {
    moved = move_me.moved;
    copied = move_me.copied;
  };
  job_queue_.Schedule(std::move(job));
  job_queue_.RunUntilIdle();

  EXPECT_FALSE(copied);
  EXPECT_TRUE(moved);
}

TEST_P(JobQueueTest, MoveOnlyLambdaIsAccepted) {
  std::atomic_bool executed = false;
  auto move_only_obj = std::make_unique<int>(42);

  job_queue_.Schedule([ptr = std::move(move_only_obj), &executed]() {
    EXPECT_EQ(*ptr, 42);
    executed = true;
  });
  job_queue_.RunUntilIdle();

  EXPECT_TRUE(executed);
}

TEST_P(JobQueueTest, NullFunctionPointerIsInvalidJob) {
  void (*null_func)() = nullptr;
  JobQueue::Job job(null_func);
  EXPECT_FALSE(job);
}

TEST_P(JobQueueTest, QueueBelongsToCorrectThread) {
  EXPECT_TRUE(job_queue_.BelongsToCurrentThread());
}

TEST_P(JobQueueTest, ReadyDelayedJobsRunBeforeLaterImmediateJobs) {
  std::vector<int> values;
  job_queue_.Schedule([&]() { values.push_back(1); }, 1 * kPrecisionUsec);
  job_queue_.Schedule([&]() { values.push_back(2); }, 2 * kPrecisionUsec);
  // Sleep past the delayed jobs, and then schedule an immediate job, which has
  // a later scheduled time than both delayed jobs.
  usleep(3 * kPrecisionUsec);
  job_queue_.Schedule([&]() { values.push_back(3); });
  job_queue_.Schedule([&]() { values.push_back(4); }, 1 * kPrecisionUsec);

  job_queue_.RunUntilIdle();
  EXPECT_THAT(values, ElementsAre(1, 2, 3));

  usleep(2 * kPrecisionUsec);
  job_queue_.RunUntilIdle();
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4));
}

TEST_P(JobQueueTest, ImmediateJobsRunBeforeLaterDelayedJobs) {
  std::vector<int> values;
  job_queue_.Schedule([&]() { values.push_back(1); });
  job_queue_.Schedule([&]() { values.push_back(2); }, 1 * kPrecisionUsec);
  job_queue_.Schedule([&]() { values.push_back(3); });

  usleep(2 * kPrecisionUsec);
  job_queue_.RunUntilIdle();
  EXPECT_THAT(values, ElementsAre(1, 3, 2));
}

TEST_P(JobQueueTest, RemoveJobByTokenRemovesImmediateAndDelayedJobs) {
  std::vector<int> values;
  auto token_1 = job_queue_.Schedule([&]() { values.push_back(1); });
  auto token_2 = job_queue_.Schedule([&]() { values.push_back(2); });
  auto token_3 = job_queue_.Schedule([&]() { values.push_back(3); });
  auto token_4 =
      job_queue_.Schedule([&]() { values.push_back(4); }, 1 * kPrecisionUsec);
  auto token_5 =
      job_queue_.Schedule([&]() { values.push_back(5); }, 1 * kPrecisionUsec);
  auto token_6 =
      job_queue_.Schedule([&]() { values.push_back(6); }, 1 * kPrecisionUsec);

  // Remove jobs from the middle, the front and the back of the queue.
  job_queue_.RemoveJobByToken(&token_2);
  job_queue_.RemoveJobByToken(&token_1);
  job_queue_.RemoveJobByToken(&token_5);
  job_queue_.RemoveJobByToken(&token_6);
  EXPECT_FALSE(token_1);
  EXPECT_FALSE(token_2);
  EXPECT_FALSE(token_5);
  EXPECT_FALSE(token_6);
  EXPECT_TRUE(token_3);
  EXPECT_TRUE(token_4);

  usleep(2 * kPrecisionUsec);
  job_queue_.RunUntilIdle();
  EXPECT_THAT(values, ElementsAre(3, 4));

  // Removing jobs that already ran is a no-op.
  job_queue_.RemoveJobByToken(&token_3);
  job_queue_.RemoveJobByToken(&token_4);
  EXPECT_FALSE(token_3);
  EXPECT_FALSE(token_4);
}

TEST_P(JobQueueTest, CancelPendingJobsCancelsImmediateAndDelayedJobs) {
  std::vector<int> values;
  job_queue_.Schedule([&]() { values.push_back(1); });
  job_owner_.Schedule([&]() { values.push_back(2); });
  job_queue_.Schedule([&]() { values.push_back(3); });
  job_owner_.Schedule([&]() { values.push_back(4); }, 1 * kPrecisionUsec);
  job_queue_.Schedule([&]() { values.push_back(5); }, 1 * kPrecisionUsec);
  job_owner_.Schedule([&]() { values.push_back(6); }, 1 * kPrecisionUsec);

  job_owner_.CancelPendingJobs();

  usleep(2 * kPrecisionUsec);
  job_queue_.RunUntilIdle();
  EXPECT_THAT(values, ElementsAre(1, 3, 5));
}

TEST_P(JobQueueTest, JobsAreDestroyedWhenRemoved) {
  struct DestructionTracker {
    DestructionTracker(int* destroyed, JobQueue* job_queue)
        : destroyed(destroyed), job_queue(job_queue) {}
    ~DestructionTracker() {
      if (destroyed) {
        ++*destroyed;
      }
      if (job_queue) {
        // Deadlocks if the job is destroyed while the queue's lock is held.
        job_queue->Schedule([] {});
      }
    }
    DestructionTracker(DestructionTracker&& other)
        : destroyed(other.destroyed), job_queue(other.job_queue) {
      other.destroyed = nullptr;
      other.job_queue = nullptr;
    }
    int* destroyed;
    JobQueue* job_queue;
  };

  // With optimizations enabled the removed jobs are destroyed after the lock
  // is released, which the tracker verifies by scheduling from its destructor.
  JobQueue* job_queue = GetParam() ? &job_queue_ : nullptr;
  int destroyed = 0;
  auto token = job_queue_.Schedule(
      [tracker = DestructionTracker(&destroyed, job_queue)] {});
  job_owner_.Schedule([tracker = DestructionTracker(&destroyed, job_queue)] {},
                      1 * kPrecisionUsec);
  job_queue_.Schedule([tracker = DestructionTracker(&destroyed, job_queue)] {},
                      1 * kPrecisionUsec);

  job_queue_.RemoveJobByToken(&token);
  EXPECT_EQ(destroyed, 1);
  job_owner_.CancelPendingJobs();
  EXPECT_EQ(destroyed, 2);
  job_queue_.StopSoon();
  EXPECT_EQ(destroyed, 3);
}

TEST_P(JobQueueTest, ScheduleAfterStopIsRejected) {
  std::atomic_bool job_1 = {false};
  job_queue_.StopSoon();
  auto token = job_queue_.Schedule([&]() { job_1 = true; });
  EXPECT_FALSE(token);
  job_queue_.RunUntilIdle();
  EXPECT_FALSE(job_1);
}

INSTANTIATE_TEST_SUITE_P(Optimizations,
                         JobQueueTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace

}  // namespace starboard
