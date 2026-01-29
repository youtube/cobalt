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

#include "starboard/shared/starboard/player/job_thread.h"

#include <condition_variable>
#include <mutex>
#include <string>

#include "starboard/common/check_op.h"
#include "starboard/thread.h"

namespace starboard {

class JobThread::WorkerThread : public Thread {
 public:
  WorkerThread(JobThread* job_thread,
               const char* thread_name,
               int64_t stack_size,
               SbThreadPriority priority)
      : Thread(thread_name, stack_size),
        job_thread_(job_thread),
        priority_(priority) {}

  void Run() override {
    SbThreadSetPriority(priority_);
    {
      std::lock_guard lock(mutex_);
      job_thread_->job_queue_ = std::make_unique<JobQueue>();
    }
    cv_.notify_one();
    job_thread_->RunLoop();
  }

  void WaitUntilJobQueueCreation() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return job_thread_->job_queue_ != nullptr; });
    SB_CHECK(job_thread_->job_queue_);
  }

 private:
  JobThread* job_thread_;
  SbThreadPriority priority_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

JobThread::JobThread(const char* thread_name,
                     int64_t stack_size,
                     SbThreadPriority priority)
    : thread_(std::make_unique<WorkerThread>(this,
                                             thread_name,
                                             stack_size,
                                             priority)) {
  thread_->Start();
  thread_->WaitUntilJobQueueCreation();
}

JobThread::~JobThread() {
  Stop();
}

void JobThread::RunLoop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  job_queue_->RunUntilStopped();
}

void JobThread::Stop() {
  if (stopped_.exchange(true, std::memory_order_release)) {
    return;
  }

  job_queue_->StopSoon();
  thread_->Join();
}

}  // namespace starboard
