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

#include <mutex>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

WidevineTimer::~WidevineTimer() {
  for (auto iter : active_clients_) {
    delete iter.second;
  }
}

void WidevineTimer::setTimeout(int64_t delay_in_milliseconds,
                               IClient* client,
                               void* context) {
  std::unique_lock lock(mutex_);
  if (active_clients_.empty()) {
    SB_CHECK(!job_thread_);
    job_thread_ = std::make_unique<JobThread>("wv_timer");
  }

  SB_CHECK(job_thread_);

  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    iter =
        active_clients_
            .emplace(client, new JobQueue::JobOwner(job_thread_->job_queue()))
            .first;
  }

  iter->second->Schedule([=]() { client->onTimerExpired(context); },
                         delay_in_milliseconds * 1000);
}

void WidevineTimer::cancel(IClient* client) {
  std::unique_lock lock(mutex_);
  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    // cancel() can be called before any timer is scheduled.
    return;
  }

  SB_CHECK(job_thread_);

  job_thread_->job_queue()->ScheduleAndWait(
      [this, client] { CancelAllJobsOnClient(client); });

  if (active_clients_.empty()) {
    // Kill the thread on the last |client|.
    job_thread_.reset();
  }
}

void WidevineTimer::CancelAllJobsOnClient(IClient* client) {
  SB_CHECK(job_thread_->BelongsToCurrentThread());

  {
    std::lock_guard lock(mutex_);
    auto iter = active_clients_.find(client);
    iter->second->CancelPendingJobs();
    delete iter->second;
    active_clients_.erase(iter);
  }
}

}  // namespace starboard
