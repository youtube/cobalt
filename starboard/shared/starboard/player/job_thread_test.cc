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

#include "starboard/shared/starboard/player/job_thread.h"

#include <atomic>
#include <vector>

#include "starboard/shared/starboard/player/job_queue.h"
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
constexpr SbTime kPrecision = kSbTimeMillisecond;

void ExecutePendingJobs(JobThread* job_thread) {
  job_thread->ScheduleAndWait([]() {});
}

TEST(JobThreadTest, ScheduledJobsAreExecutedInOrder) {
  std::vector<int> values;
  JobThread job_thread{"JobThreadTests"};
  job_thread.Schedule([&]() { values.push_back(1); });
  job_thread.Schedule([&]() { values.push_back(2); });
  job_thread.Schedule([&]() { values.push_back(3); });
  job_thread.Schedule([&]() { values.push_back(4); }, 1 * kPrecision);
  job_thread.Schedule([&]() { values.push_back(5); }, 1 * kPrecision);
  job_thread.Schedule([&]() { values.push_back(6); }, 1 * kPrecision);
  job_thread.Schedule([&]() { values.push_back(7); }, 2 * kPrecision);
  job_thread.Schedule([&]() { values.push_back(8); }, 3 * kPrecision);

  // Sleep past the last scheduled job.
  SbThreadSleep(4 * kPrecision);

  ExecutePendingJobs(&job_thread);

  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(JobThreadTest, ScheduleAndWaitWaits) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  std::atomic_bool job_1 = {false};
  JobThread job_thread{"JobThreadTests"};
  job_thread.ScheduleAndWait([&]() {
    SbThreadSleep(1 * kPrecision);
    job_1 = true;
  });
  // Verify that the job ran and that it took at least as long as it slept.
  EXPECT_TRUE(job_1);
  EXPECT_GE(SbTimeGetMonotonicNow() - start, 1 * kPrecision);
}

TEST(JobThreadTest, ScheduledJobsShouldNotExecuteAfterGoingOutOfScope) {
  std::atomic_int counter = {0};
  {
    JobThread job_thread{"JobThreadTests"};
    std::function<void()> job = [&]() {
      counter++;
      job_thread.Schedule(job, 2 * kPrecision);
    };
    job_thread.Schedule(job);

    // Wait for the job to run at least once and reschedule itself.
    SbThreadSleep(1 * kPrecision);
    ExecutePendingJobs(&job_thread);
  }
  int end_value = counter;
  EXPECT_GE(counter, 1);

  // Sleep past two more (potential) executions and verify there were none.
  SbThreadSleep(4 * kPrecision);
  EXPECT_EQ(counter, end_value);
}

TEST(JobThreadTest, CanceledJobsAreCanceled) {
  std::atomic_int counter_1 = {0}, counter_2 = {0};
  JobQueue::JobToken job_token_1, job_token_2;

  JobThread job_thread{"JobThreadTests"};
  std::function<void()> job_1 = [&]() {
    counter_1++;
    job_token_1 = job_thread.Schedule(job_1);
  };
  std::function<void()> job_2 = [&]() {
    counter_2++;
    job_token_2 = job_thread.Schedule(job_2);
  };

  job_token_1 = job_thread.Schedule(job_1);
  job_token_2 = job_thread.Schedule(job_2);

  // Wait for the scheduled jobs to at least run once.
  ExecutePendingJobs(&job_thread);

  // Cancel job 1 and grab the current counter values.
  job_thread.ScheduleAndWait(
      [&]() { job_thread.RemoveJobByToken(job_token_1); });
  int checkpoint_1 = counter_1;
  int checkpoint_2 = counter_2;

  // Sleep and wait for pending jobs to run.
  SbThreadSleep(1 * kPrecision);
  ExecutePendingJobs(&job_thread);

  // Job 1 should not have run again.
  EXPECT_EQ(counter_1, checkpoint_1);

  // Job 2 should have continued running.
  EXPECT_GT(counter_2, checkpoint_2);

  // Cancel job 2 to avoid it scheduling itself during destruction.
  job_thread.ScheduleAndWait(
      [&]() { job_thread.RemoveJobByToken(job_token_2); });
}

TEST(JobThreadTest, QueueBelongsToCorrectThread) {
  JobThread job_thread{"JobThreadTests"};
  JobQueue job_queue;

  bool belongs_to_job_thread = false;
  bool belongs_to_main_thread = false;

  // Schedule in JobQueue owned by job thread.
  job_thread.ScheduleAndWait([&]() {
    belongs_to_job_thread = job_thread.BelongsToCurrentThread();
    belongs_to_main_thread = job_queue.BelongsToCurrentThread();
  });

  EXPECT_TRUE(belongs_to_job_thread);
  EXPECT_FALSE(belongs_to_main_thread);

  belongs_to_job_thread = belongs_to_main_thread = false;

  // Schedule in JobQueue owned by main thread.
  job_queue.Schedule([&]() {
    belongs_to_job_thread = job_thread.BelongsToCurrentThread();
    belongs_to_main_thread = job_queue.BelongsToCurrentThread();
  });

  job_queue.RunUntilIdle();
  EXPECT_FALSE(belongs_to_job_thread);
  EXPECT_TRUE(belongs_to_main_thread);
}

}  // namespace
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
