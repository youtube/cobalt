// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"
#include "starboard/once.h"
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

JobQueue::JobQueue() : thread_id_(SbThreadGetId()), stopped_(false) {
  SB_DCHECK(SbThreadIsValidId(thread_id_));
  SetCurrentThreadJobQueue(this);
}

JobQueue::~JobQueue() {
  SB_DCHECK(BelongsToCurrentThread());
  StopSoon();
  ResetCurrentThreadJobQueue();
}

void JobQueue::Schedule(Closure job, SbTimeMonotonic delay /*= 0*/) {
  SB_DCHECK(job.is_valid());
  SB_DCHECK(delay >= 0) << delay;

  ScopedLock scoped_lock(mutex_);
  if (stopped_) {
    return;
  }
  if (delay <= 0) {
    queue_.Put(job);
    return;
  }
  SbTimeMonotonic time_to_run_job = SbTimeGetMonotonicNow() + delay;
  time_to_job_map_.insert(std::make_pair(time_to_run_job, job));
  if (time_to_job_map_.begin()->second == job) {
    queue_.Wake();
  }
}

void JobQueue::Remove(Closure job) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(job.is_valid());

  ScopedLock scoped_lock(mutex_);
  queue_.Remove(job);
  // std::multimap::erase() doesn't return an iterator until C++11.  So this has
  // to be done in a nested loop to delete multiple occurrences of |job|.
  bool should_keep_running = true;
  while (should_keep_running) {
    should_keep_running = false;
    for (TimeToJobMap::iterator iter = time_to_job_map_.begin();
         iter != time_to_job_map_.end(); ++iter) {
      if (iter->second == job) {
        time_to_job_map_.erase(iter);
        should_keep_running = true;
        break;
      }
    }
  }
}

void JobQueue::StopSoon() {
  {
    ScopedLock scoped_lock(mutex_);
    stopped_ = true;
  }

  queue_.Wake();

  for (;;) {
    Closure job = queue_.Poll();
    if (!job.is_valid()) {
      break;
    }
  }
  time_to_job_map_.clear();
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

bool JobQueue::TryToRunOneJob(bool wait_for_next_job) {
  SB_DCHECK(BelongsToCurrentThread());

  Closure job;
  // |kSbTimeMax| makes more sense here, but |kSbTimeDay| is much safer.
  SbTimeMonotonic delay = kSbTimeDay;

  {
    ScopedLock scoped_lock(mutex_);
    if (stopped_) {
      return false;
    }
    if (!time_to_job_map_.empty()) {
      TimeToJobMap::iterator first_delayed_job = time_to_job_map_.begin();
      delay = first_delayed_job->first - SbTimeGetMonotonicNow();
      if (delay <= 0) {
        job = first_delayed_job->second;
        time_to_job_map_.erase(first_delayed_job);
      }
    }
  }

  if (!job.is_valid()) {
    if (wait_for_next_job) {
      job = queue_.GetTimed(delay);
    } else {
      job = queue_.Poll();
    }
  }

  if (!job.is_valid()) {
    return false;
  }

  job.Run();
  return true;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
