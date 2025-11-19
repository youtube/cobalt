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
               SbThreadPriority priority,
               std::mutex* mutex,
               std::condition_variable* cv)
      : Thread(thread_name, stack_size),
        job_thread_(job_thread),
        priority_(priority),
        mutex_(mutex),
        cv_(cv) {}

  void Run() override {
    SbThreadSetPriority(priority_);
    {
      std::lock_guard lock(*mutex_);
      job_thread_->job_queue_ = std::make_unique<JobQueue>();
    }
    cv_->notify_one();
    job_thread_->RunLoop();
  }

 private:
  JobThread* job_thread_;
  SbThreadPriority priority_;
  std::mutex* mutex_;
  std::condition_variable* cv_;
};

JobThread::JobThread(const char* thread_name,
                     int64_t stack_size,
                     SbThreadPriority priority) {
  std::mutex mutex;
  std::condition_variable condition_variable;

  thread_ = std::make_unique<WorkerThread>(
      this, thread_name, stack_size, priority, &mutex, &condition_variable);
  thread_->Start();

  std::unique_lock lock(mutex);
  condition_variable.wait(lock, [this] { return job_queue_ != nullptr; });
  SB_DCHECK(job_queue_);
}

JobThread::~JobThread() {
  // TODO: There is a potential race condition here since job_queue_ can get
  // reset if it's is stopped while this dtor is running. Thus, avoid stopping
  // job_queue_ before JobThread is destructed.
  if (job_queue_) {
    job_queue_->Schedule(std::bind(&JobQueue::StopSoon, job_queue_.get()));
  }
  thread_->Join();
}

void JobThread::RunLoop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  job_queue_->RunUntilStopped();
  // TODO: Investigate removing this line to avoid the race condition in the
  // dtor.
  job_queue_.reset();
}

}  // namespace starboard
