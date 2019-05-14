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

#include <utility>

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/system.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

JobQueue* GetCurrentThreadJobQueue() {
  EnsureThreadLocalKeyInited();
  return static_cast<JobQueue*>(SbThreadGetLocalValue(s_thread_local_key));
}

void SetCurrentThreadJobQueue(JobQueue* job_queue) {
  SB_DCHECK(job_queue != NULL);
  SB_DCHECK(GetCurrentThreadJobQueue() == NULL);

  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, job_queue);
}

void ResetCurrentThreadJobQueue() {
  SB_DCHECK(GetCurrentThreadJobQueue());

  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, NULL);
}

}  // namespace

JobQueue::JobQueue() : thread_id_(SbThreadGetId()), condition_(mutex_) {
  SB_DCHECK(SbThreadIsValidId(thread_id_));
  SetCurrentThreadJobQueue(this);
}

JobQueue::~JobQueue() {
  SB_DCHECK(BelongsToCurrentThread());
  StopSoon();
  ResetCurrentThreadJobQueue();
}

JobQueue::JobToken JobQueue::Schedule(Job job, SbTimeMonotonic delay /*= 0*/) {
  return Schedule(job, NULL, delay);
}

void JobQueue::RemoveJobByToken(JobToken job_token) {
  SB_DCHECK(BelongsToCurrentThread());

  if (!job_token.is_valid()) {
    return;
  }

  ScopedLock scoped_lock(mutex_);
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
    ScopedLock scoped_lock(mutex_);
    stopped_ = true;
    time_to_job_record_map_.clear();
    condition_.Signal();
  }
}

void JobQueue::RunUntilStopped() {
  SB_DCHECK(BelongsToCurrentThread());

  for (;;) {
    {
      ScopedLock scoped_lock(mutex_);
      if (stopped_) {
        return;
      }
    }
    TryToRunOneJob(/*wait_for_next_job =*/true);
  }
}

void JobQueue::RunUntilIdle() {
  SB_DCHECK(BelongsToCurrentThread());

  while (TryToRunOneJob(/*wait_for_next_job =*/false)) {
  }
}

bool JobQueue::BelongsToCurrentThread() const {
  // The ctor already ensures that the current JobQueue is the only JobQueue of
  // the thread, checking for thread id is more light-weighted then calling
  // JobQueue::current() and compare the result with |this|.
  return thread_id_ == SbThreadGetId();
}

// static
JobQueue* JobQueue::current() {
  return GetCurrentThreadJobQueue();
}

JobQueue::JobToken JobQueue::Schedule(Job job,
                                      JobOwner* owner,
                                      SbTimeMonotonic delay) {
  SB_DCHECK(job);
  SB_DCHECK(delay >= 0) << delay;

  ScopedLock scoped_lock(mutex_);
  if (stopped_) {
    return JobToken();
  }

  ++current_job_token_;

  JobToken job_token(current_job_token_);
  JobRecord job_record = {job_token, job, owner};
#if ENABLE_JOB_QUEUE_PROFILING
  job_record.stack_size =
      SbSystemGetStack(job_record.stack, kProfileStackDepth);
#endif  // ENABLE_JOB_QUEUE_PROFILING

  SbTimeMonotonic time_to_run_job = SbTimeGetMonotonicNow() + delay;
  bool is_first_job = time_to_job_record_map_.empty() ||
                      time_to_run_job < time_to_job_record_map_.begin()->first;

  time_to_job_record_map_.insert(std::make_pair(time_to_run_job, job_record));
  if (is_first_job) {
    condition_.Signal();
  }
  return job_token;
}

void JobQueue::RemoveJobsByOwner(JobOwner* owner) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(owner);

  ScopedLock scoped_lock(mutex_);
  // std::multimap::erase() doesn't return an iterator until C++11.  So this has
  // to be done in a nested loop to delete multiple occurrences of |job|.
  bool should_keep_running = true;
  while (should_keep_running) {
    should_keep_running = false;
    for (TimeToJobRecordMap::iterator iter = time_to_job_record_map_.begin();
         iter != time_to_job_record_map_.end(); ++iter) {
      if (iter->second.owner == owner) {
        time_to_job_record_map_.erase(iter);
        should_keep_running = true;
        break;
      }
    }
  }
}

bool JobQueue::TryToRunOneJob(bool wait_for_next_job) {
  SB_DCHECK(BelongsToCurrentThread());

  JobRecord job_record;

  {
    ScopedLock scoped_lock(mutex_);
    if (stopped_) {
      return false;
    }
    if (time_to_job_record_map_.empty() && wait_for_next_job) {
      // |kSbTimeMax| makes more sense here, but |kSbTimeDay| is much safer.
      condition_.WaitTimed(kSbTimeDay);
    }
    if (time_to_job_record_map_.empty()) {
      return false;
    }
    TimeToJobRecordMap::iterator first_delayed_job =
        time_to_job_record_map_.begin();
    SbTimeMonotonic delay = first_delayed_job->first - SbTimeGetMonotonicNow();
    if (delay > 0) {
      if (wait_for_next_job) {
        condition_.WaitTimed(delay);
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
    delay = first_delayed_job->first - SbTimeGetMonotonicNow();
    if (delay > 0) {
      return false;
    }
    job_record = first_delayed_job->second;
    time_to_job_record_map_.erase(first_delayed_job);
  }

  SB_DCHECK(job_record.job);

#if ENABLE_JOB_QUEUE_PROFILING
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
#endif  // ENABLE_JOB_QUEUE_PROFILING

  job_record.job();

#if ENABLE_JOB_QUEUE_PROFILING
  auto now = SbTimeGetMonotonicNow();
  auto elapsed = now - start;
  if (elapsed > max_job_interval_) {
    job_record_with_max_interval_ = job_record;
    max_job_interval_ = elapsed;
  }
  if (now - last_reset_time_ > kProfileResetInterval) {
    SB_LOG(INFO) << "================ Max job takes " << max_job_interval_;
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
  }
#endif  // ENABLE_JOB_QUEUE_PROFILING
  return true;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
