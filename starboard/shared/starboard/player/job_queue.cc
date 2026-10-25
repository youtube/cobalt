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

#include <atomic>
#include <chrono>
#include <memory>
#include <type_traits>

#include "starboard/system.h"

namespace starboard {

namespace {

constexpr bool kAreOptimizationsDefaultEnabled = false;

static_assert(std::is_trivially_destructible<std::atomic<bool>>::value,
              "g_enable_optimizations must be trivially destructible.");
// Latched by every JobQueue in its constructor, see EnableOptimizations().
std::atomic<bool> g_enable_optimizations{kAreOptimizationsDefaultEnabled};

}  // namespace

const JobQueue::JobToken JobQueue::JobToken::kUnscheduled =
    JobQueue::JobToken();

JobQueue::JobToken JobQueue::JobToken::Generate() {
  static std::atomic<int64_t> s_current_token{0};
  return JobToken(s_current_token.fetch_add(1, std::memory_order_relaxed) + 1);
}

JobQueue::JobQueue()
    : optimizations_enabled_(
          g_enable_optimizations.load(std::memory_order_acquire)) {}

JobQueue::~JobQueue() {
  StopSoon();
}

// static
void JobQueue::EnableOptimizations() {
  g_enable_optimizations.store(true, std::memory_order_release);
}

JobQueue::ScopedOptimizationsForTesting::ScopedOptimizationsForTesting(
    bool enabled)
    : previous_value_(
          g_enable_optimizations.exchange(enabled, std::memory_order_acq_rel)) {
}

JobQueue::ScopedOptimizationsForTesting::~ScopedOptimizationsForTesting() {
  g_enable_optimizations.store(previous_value_, std::memory_order_release);
}

JobQueue::JobToken JobQueue::Schedule(Job job, int64_t delay_usec /*= 0*/) {
  return Schedule(std::move(job), /*owner=*/nullptr, delay_usec);
}

void JobQueue::ScheduleAndWait(Job job) {
  // TODO: Allow calling from the JobQueue thread.
  SB_CHECK(!BelongsToCurrentThread());

  if (optimizations_enabled_) {
    // A single job runs the caller's job, destroys it (so its captures are
    // gone before the caller is released, like with the original two jobs)
    // and then signals its completion through a heap allocated flag, so it
    // never accesses the caller's stack, even when this function returns
    // early because the queue is stopped while the job is running.  The
    // caller waits on |sync_condition_|, which keeps the worker the only
    // waiter of |condition_| (see Schedule()).
    auto finished = std::make_shared<bool>(false);
    Schedule([this, finished, job = std::move(job)]() mutable {
      job();
      job = nullptr;
      {
        std::lock_guard lock(mutex_);
        *finished = true;
      }
      sync_condition_.notify_all();
    });

    std::unique_lock lock(mutex_);
    sync_condition_.wait(lock, [&] { return *finished || stopped_; });
    return;
  }

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

void JobQueue::RemoveJobByToken(JobToken* job_token) {
  SB_CHECK(BelongsToCurrentThread());

  if (!*job_token) {
    return;
  }

  {
    std::lock_guard lock(mutex_);
    for (TimeToJobRecordMap::iterator iter = time_to_job_record_map_.begin();
         iter != time_to_job_record_map_.end(); ++iter) {
      if (iter->second.job_token == *job_token) {
        time_to_job_record_map_.erase(iter);
        break;
      }
    }
  }
  job_token->Reset();
}

void JobQueue::StopSoon() {
  {
    std::lock_guard lock(mutex_);
    stopped_ = true;
    time_to_job_record_map_.clear();
  }
  condition_.notify_all();
  if (optimizations_enabled_) {
    sync_condition_.notify_all();
  }
}

void JobQueue::RunUntilStopped() {
  SB_CHECK(BelongsToCurrentThread());

  if (optimizations_enabled_) {
    // |stopped_| is atomic, so there is no need to take the lock for every job
    // just to check it; TryToRunOneJob() checks it under the lock anyway.
    while (!stopped_.load(std::memory_order_relaxed)) {
      TryToRunOneJob(/*wait_for_next_job =*/true);
    }
    return;
  }

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

JobQueue::JobToken JobQueue::Schedule(Job job,
                                      JobOwner* owner,
                                      int64_t delay_usec) {
  SB_DCHECK(job);
  SB_DCHECK_GE(delay_usec, 0);

  std::unique_lock lock(mutex_);
  if (stopped_) {
    return JobToken::kUnscheduled;
  }

  JobToken job_token = JobToken::Generate();
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
  if (!optimizations_enabled_) {
    if (is_first_job) {
      condition_.notify_all();
    }
    return job_token;
  }

  // The worker only has to be woken up when it is actually waiting, which
  // avoids a syscall for every job scheduled while it is busy (in particular
  // for the jobs it schedules to itself).  It is signaled after the lock is
  // released, so that it doesn't immediately block on the mutex still held by
  // this thread.  The worker is the only waiter of |condition_|, as
  // ScheduleAndWait() waits on |sync_condition_| when optimizations are
  // enabled.
  const bool wake_worker = is_first_job && worker_waiting_;
  lock.unlock();
  if (wake_worker) {
    condition_.notify_one();
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
      worker_waiting_ = true;
      condition_.wait(lock, [this] {
        return stopped_ || !time_to_job_record_map_.empty();
      });
      worker_waiting_ = false;
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
        worker_waiting_ = true;
        condition_.wait_for(
            lock, std::chrono::microseconds(delay_usec),
            [this, scheduled_time] {
              return stopped_ || time_to_job_record_map_.empty() ||
                     time_to_job_record_map_.begin()->first != scheduled_time;
            });
        worker_waiting_ = false;
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
    // Only the stack is logged, so don't keep the job (and whatever it
    // captures) alive until a slower job comes along.
    job_record.job = nullptr;
    job_record_with_max_interval_ = std::move(job_record);
    max_job_interval_ = elapsed;
  }
  if (now - last_reset_time_ > kProfileResetIntervalUsec) {
    SB_LOG(INFO) << "================ " << jobs_processed_
                 << " jobs processed, and waited for " << wait_times_
                 << " times since last reset on 0x" << this
                 << ", max job takes " << max_job_interval_;
    for (int i = 0; i < job_record_with_max_interval_.stack_size; ++i) {
      char function_name[1024];
      // Subtract one as return address of function may be in the next function
      // when a function is annotated as noreturn.
      const void* call =
          static_cast<const char*>(job_record_with_max_interval_.stack[i]) - 1;
      if (SbSystemSymbolize(call, function_name,
                            SB_ARRAY_SIZE_INT(function_name))) {
        SB_LOG(INFO) << "    " << function_name;
      } else {
        SB_LOG(INFO) << "    " << job_record_with_max_interval_.stack[i];
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
