// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/widevine/widevine_timer.h"

#include "starboard/common/log.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace widevine {

namespace {

struct ThreadParam {
  WidevineTimer* timer;
  ConditionVariable* condition_variable;
};

}  // namespace

WidevineTimer::~WidevineTimer() {
  for (auto iter : active_clients_) {
    delete iter.second;
  }
}

void WidevineTimer::setTimeout(int64_t delay_in_milliseconds,
                               IClient* client,
                               void* context) {
  ScopedLock scoped_lock(mutex_);
  if (active_clients_.empty()) {
    SB_DCHECK(!SbThreadIsValid(thread_));
    SB_DCHECK(!job_queue_);

    ConditionVariable condition_variable(mutex_);
    ThreadParam thread_param = {this, &condition_variable};
    thread_ =
        SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                       "wv_timer", &WidevineTimer::ThreadFunc, &thread_param);
    condition_variable.Wait();
  }

  SB_DCHECK(SbThreadIsValid(thread_));
  SB_DCHECK(job_queue_);

  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    iter = active_clients_.emplace(client, new JobQueue::JobOwner(job_queue_))
               .first;
  }

  iter->second->Schedule([=]() { client->onTimerExpired(context); },
                         delay_in_milliseconds * kSbTimeMillisecond);
}

void WidevineTimer::cancel(IClient* client) {
  ScopedLock scoped_lock(mutex_);
  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    // cancel() can be called before any timer is scheduled.
    return;
  }

  SB_DCHECK(job_queue_);

  ConditionVariable condition_variable(mutex_);
  job_queue_->Schedule(
      [&]() { CancelAllJobsOnClient(client, &condition_variable); });
  condition_variable.Wait();

  if (active_clients_.empty()) {
    // Kill the thread on the last |client|.
    job_queue_->StopSoon();
    SbThreadJoin(thread_, NULL);
    thread_ = kSbThreadInvalid;
    job_queue_ = NULL;
  }
}

// static
void* WidevineTimer::ThreadFunc(void* param) {
  SB_DCHECK(param);
  ThreadParam* thread_param = static_cast<ThreadParam*>(param);
  thread_param->timer->RunLoop(thread_param->condition_variable);
  return NULL;
}

void WidevineTimer::RunLoop(ConditionVariable* condition_variable) {
  SB_DCHECK(!job_queue_);
  SB_DCHECK(condition_variable);

  JobQueue job_queue;

  {
    ScopedLock scoped_lock(mutex_);
    job_queue_ = &job_queue;
    condition_variable->Signal();
  }

  job_queue.RunUntilStopped();
}

void WidevineTimer::CancelAllJobsOnClient(
    IClient* client,
    ConditionVariable* condition_variable) {
  SB_DCHECK(condition_variable);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  ScopedLock scoped_lock(mutex_);
  auto iter = active_clients_.find(client);
  iter->second->CancelPendingJobs();
  delete iter->second;
  active_clients_.erase(iter);
  condition_variable->Signal();
}

}  // namespace widevine
}  // namespace shared
}  // namespace starboard
