// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <chrono>
#include <limits>
#include <mutex>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/system.h"
#include "starboard/thread.h"

namespace starboard {

JobQueue::JobQueue() = default;

JobQueue::~JobQueue() {
  SB_CHECK(BelongsToCurrentThread());
  StopSoon();
}

JobQueue::JobToken JobQueue::Schedule(const Job& job,
                                      int64_t delay_usec /*= 0*/) {
  return Schedule(job, NULL, delay_usec);
}

JobQueue::JobToken JobQueue::Schedule(Job&& job, int64_t delay_usec /*= 0*/) {
  return Schedule(std::move(job), NULL, delay_usec);
}

void JobQueue::ScheduleAndWait(const Job& job) {
  ScheduleAndWait(Job(job));
}

void JobQueue::ScheduleAndWait(Job&& job) {
  // TODO: Allow calling from the JobQueue thread.
  SB_CHECK(!BelongsToCurrentThread());

  Schedule(std::move(job));

  bool job_finished = false;

  Schedule([&]() {
    {
      std::lock_guard lock(mutex_);
      job_finished = true;
    }
    condition_.notify_all();
  });

  std::unique_lock lock(mutex_);
  condition_.wait(lock, [&] { return job_finished || stopped_; });
}

void JobQueue::RemoveJobByToken(JobToken job_token) {
  SB_CHECK(BelongsToCurrentThread());

  if (!job_token.is_valid()) {
    return;
  }

  std::lock_guard lock(mutex_);
  for (TimeToJobRecordMap::iterator iter = time_to_job_record_map_.begin();
       iter != time_to_job_record_map_.end(); ++iter) {
    if (iter->second.job_token == job_token) {
      time_to_job_record_map_.erase(iter);
      return;
    }
  }
}

void JobQueue::StopSoon() {
  {
    std::lock_guard lock(mutex_);
    stopped_ = true;
    time_to_job_record_map_.clear();
  }
  condition_.notify_all();
}

void JobQueue::RunUntilStopped() {
  SB_CHECK(BelongsToCurrentThread());

  for (;;) {
    {
      std::unique_lock lock(mutex_);
      if (stopped_) {
        return;
      }
    }
    TryToRunOneJob(/*wait_for_next_job =*/true);
  }
}

void JobQueue::RunUntilIdle() {
  SB_CHECK(BelongsToCurrentThread());

  while (TryToRunOneJob(/*wait_for_next_job =*/false)) {
  }
}

bool JobQueue::BelongsToCurrentThread() const {
  return thread_checker_.CalledOnValidThread();
}

JobQueue::JobToken JobQueue::Schedule(const Job& job,
                                      JobOwner* owner,
                                      int64_t delay_usec) {
  return Schedule(Job(job), owner, delay_usec);
}

JobQueue::JobToken JobQueue::Schedule(Job&& job,
                                      JobOwner* owner,
                                      int64_t delay_usec) {
  SB_DCHECK(job);
  SB_DCHECK_GE(delay_usec, 0);

  std::lock_guard lock(mutex_);
  if (stopped_) {
    return JobToken();
  }

  ++current_job_token_;

  JobToken job_token(current_job_token_);
  JobRecord job_record = {job_token, std::move(job), owner};
#if ENABLE_JOB_QUEUE_PROFILING
  if (kProfileStackDepth > 0) {
    job_record.stack_size =
        SbSystemGetStack(job_record.stack, kProfileStackDepth);
  }
#endif  // ENABLE_JOB_QUEUE_PROFILING

  int64_t time_to_run_job = CurrentMonotonicTime() + delay_usec;
  bool is_first_job = time_to_job_record_map_.empty() ||
                      time_to_run_job < time_to_job_record_map_.begin()->first;

  time_to_job_record_map_.insert({time_to_run_job, std::move(job_record)});
  if (is_first_job) {
    condition_.notify_all();
  }
  return job_token;
}

void JobQueue::RemoveJobsByOwner(JobOwner* owner) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(owner);

  std::lock_guard lock(mutex_);
  for (TimeToJobRecordMap::iterator iter = time_to_job_record_map_.begin();
       iter != time_to_job_record_map_.end();) {
    if (iter->second.owner == owner) {
      iter = time_to_job_record_map_.erase(iter);
    } else {
      ++iter;
    }
  }
}

bool JobQueue::TryToRunOneJob(bool wait_for_next_job) {
  SB_CHECK(BelongsToCurrentThread());

  JobRecord job_record;

  {
    std::unique_lock lock(mutex_);
    if (stopped_) {
      return false;
    }
    if (time_to_job_record_map_.empty() && wait_for_next_job) {
      condition_.wait(lock, [this] {
        return stopped_ || !time_to_job_record_map_.empty();
      });
#if ENABLE_JOB_QUEUE_PROFILING
      ++wait_times_;
#endif  // ENABLE_JOB_QUEUE_PROFILING
    }
    if (time_to_job_record_map_.empty()) {
      return false;
    }
    TimeToJobRecordMap::iterator first_delayed_job =
        time_to_job_record_map_.begin();
    int64_t delay_usec = first_delayed_job->first - CurrentMonotonicTime();
    if (delay_usec > 0) {
      if (wait_for_next_job) {
        int64_t scheduled_time = first_delayed_job->first;
        condition_.wait_for(
            lock, std::chrono::microseconds(delay_usec),
            [this, scheduled_time] {
              return stopped_ || time_to_job_record_map_.empty() ||
                     time_to_job_record_map_.begin()->first != scheduled_time;
            });
#if ENABLE_JOB_QUEUE_PROFILING
        ++wait_times_;
#endif  // ENABLE_JOB_QUEUE_PROFILING
        if (time_to_job_record_map_.empty()) {
          return false;
        }
      } else {
        return false;
      }
    }
    // Try to retrieve the job again as the job map can be altered during the
    // wait.
    first_delayed_job = time_to_job_record_map_.begin();
    delay_usec = first_delayed_job->first - CurrentMonotonicTime();
    if (delay_usec > 0) {
      return false;
    }
    job_record = std::move(first_delayed_job->second);
    time_to_job_record_map_.erase(first_delayed_job);
  }

  SB_DCHECK(job_record.job);

#if ENABLE_JOB_QUEUE_PROFILING
  int64_t start = CurrentMonotonicTime();
#endif  // ENABLE_JOB_QUEUE_PROFILING

  job_record.job();

#if ENABLE_JOB_QUEUE_PROFILING
  ++jobs_processed_;

  auto now = CurrentMonotonicTime();
  auto elapsed = now - start;
  if (elapsed > max_job_interval_) {
    job_record_with_max_interval_ = job_record;
    max_job_interval_ = elapsed;
  }
  if (now - last_reset_time_ > kProfileResetIntervalUsec) {
    SB_LOG(INFO) << "================ " << jobs_processed_
                 << " jobs processed, and waited for " << wait_times_
                 << " times since last reset on 0x" << this
                 << ", max job takes " << max_job_interval_;
    for (int i = 0; i < job_record.stack_size; ++i) {
      char function_name[1024];
      if (SbSystemSymbolize(job_record.stack[i], function_name,
                            SB_ARRAY_SIZE_INT(function_name))) {
        SB_LOG(INFO) << "    " << function_name;
      } else {
        SB_LOG(INFO) << "    " << job_record.stack[i];
      }
    }
    last_reset_time_ = now;
    max_job_interval_ = 0;
    jobs_processed_ = 0;
    wait_times_ = 0;
  }
#endif  // ENABLE_JOB_QUEUE_PROFILING
  return true;
}

}  // namespace starboard
