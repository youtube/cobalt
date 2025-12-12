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
#include "starboard/common/thread.h"

namespace starboard {

class WidevineTimer::WaitEvent {
 public:
  explicit WaitEvent(std::mutex& mutex) : mutex_(mutex) {}

  void Wait(std::unique_lock<std::mutex>& lock) {
    cv_.wait(lock, [this] { return is_signaled_; });
  }

  void Signal() {
    {
      std::lock_guard lock(mutex_);
      is_signaled_ = true;
    }
    cv_.notify_one();
  }

 private:
  std::mutex& mutex_;
  std::condition_variable cv_;
  bool is_signaled_ = false;  // Guarded by |mutex_|.
};

namespace {

class TimerThread : public Thread {
 public:
  TimerThread(WidevineTimer* timer, WidevineTimer::WaitEvent* wait_event)
      : Thread("wv_timer"), timer_(timer), wait_event_(wait_event) {}

  void Run() override { timer_->RunLoop(wait_event_); }

 private:
  WidevineTimer* timer_;
  WidevineTimer::WaitEvent* wait_event_;
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
  std::unique_lock lock(mutex_);
  if (active_clients_.empty()) {
    SB_DCHECK(!thread_);
    SB_DCHECK(!job_queue_);

    WaitEvent wait_event(mutex_);
    thread_ = std::make_unique<TimerThread>(this, &wait_event);
    thread_->Start();
    wait_event.Wait(lock);
  }

  SB_DCHECK(thread_);
  SB_DCHECK(job_queue_);

  auto iter = active_clients_.find(client);
  if (iter == active_clients_.end()) {
    iter = active_clients_.emplace(client, new JobQueue::JobOwner(job_queue_))
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

  SB_DCHECK(job_queue_);

  WaitEvent wait_event(mutex_);
  job_queue_->Schedule([&]() { CancelAllJobsOnClient(client, &wait_event); });
  wait_event.Wait(lock);

  if (active_clients_.empty()) {
    // Kill the thread on the last |client|.
    job_queue_->StopSoon();
    thread_->Join();
    thread_.reset();
    job_queue_ = NULL;
  }
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
    iter->second->CancelPendingJobs();
    delete iter->second;
    active_clients_.erase(iter);
  }
  wait_event->Signal();
}

}  // namespace starboard
