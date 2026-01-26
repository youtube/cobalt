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

#include <condition_variable>
#include <mutex>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {

WidevineTimer::~WidevineTimer() {
  std::lock_guard lock(mutex_);
  if (job_thread_) {
    // Flush any pending tasks before deleting |active_clients_|. This ensures
    // that background tasks (like those scheduled by cancel()) that capture
    // iterators into |active_clients_| finish before the data is destroyed.
    // TODO: b/477902972 - Call stop method instead, once it's added.
    job_thread_->ScheduleAndWait([] {});
  }
  for (auto iter : active_clients_) {
    delete iter.second;
  }
}

void WidevineTimer::setTimeout(int64_t delay_in_milliseconds,
                               IClient* client,
                               void* context) {
  std::unique_lock lock(mutex_);
  if (active_clients_.empty()) {
<<<<<<< HEAD
    SB_DCHECK_EQ(thread_, 0);
    SB_DCHECK(!job_queue_);

    WaitEvent wait_event(mutex_);
    ThreadParam thread_param = {this, &wait_event};
    pthread_create(&thread_, nullptr, &WidevineTimer::ThreadFunc,
                   &thread_param);
    wait_event.Wait(lock);
  }

  SB_DCHECK_NE(thread_, 0);
  SB_DCHECK(job_queue_);
=======
    SB_CHECK(!job_thread_);
    job_thread_ = std::make_unique<JobThread>("wv_timer");
  }

  SB_CHECK(job_thread_);
>>>>>>> 1ddaea6b90 (starboard: Refactor WidevineTimer to use JobThread (#8690))

  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    iter =
        active_clients_
            .emplace(client, new JobQueue::JobOwner(job_thread_->job_queue()))
            .first;
  }

  iter->second->Schedule([client, context] { client->onTimerExpired(context); },
                         delay_in_milliseconds * 1000);
}

void WidevineTimer::cancel(IClient* client) {
  std::lock_guard lock(mutex_);
  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    // cancel() can be called before any timer is scheduled.
    return;
  }

  SB_CHECK(job_thread_);

<<<<<<< HEAD
  WaitEvent wait_event(mutex_);
  job_queue_->Schedule([&]() { CancelAllJobsOnClient(client, &wait_event); });
  wait_event.Wait(lock);

  if (active_clients_.empty()) {
    // Kill the thread on the last |client|.
    job_queue_->StopSoon();
    pthread_join(thread_, NULL);
    thread_ = 0;
    job_queue_ = NULL;
  }
}

// static
void* WidevineTimer::ThreadFunc(void* param) {
  SB_DCHECK(param);
  pthread_setname_np(pthread_self(), "wv_timer");
  ThreadParam* thread_param = static_cast<ThreadParam*>(param);
  thread_param->timer->RunLoop(thread_param->wait_event);
  return NULL;
}

void WidevineTimer::RunLoop(WaitEvent* wait_event) {
  SB_DCHECK(!job_queue_);
  SB_DCHECK(wait_event);

  JobQueue job_queue;

  {
    std::lock_guard lock(mutex_);
    job_queue_ = &job_queue;
  }
  wait_event->Signal();

  job_queue.RunUntilStopped();
}

void WidevineTimer::CancelAllJobsOnClient(IClient* client,
                                          WaitEvent* wait_event) {
  SB_DCHECK(wait_event);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  {
    std::lock_guard lock(mutex_);
    auto iter = active_clients_.find(client);
=======
  job_thread_->ScheduleAndWait([this, iter] {
>>>>>>> 1ddaea6b90 (starboard: Refactor WidevineTimer to use JobThread (#8690))
    iter->second->CancelPendingJobs();
    delete iter->second;
    active_clients_.erase(iter);
  });

  if (active_clients_.empty()) {
    // Kill the thread on the last |client|.
    job_thread_.reset();
  }
}

}  // namespace starboard
