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
  WorkerThread(std::string_view thread_name, SbThreadPriority priority)
      : Thread(std::string(thread_name)), priority_(priority) {}

  void Run() override {
    SbThreadSetPriority(priority_);
    auto job_queue = std::make_unique<JobQueue>();
    JobQueue* job_queue_ptr = job_queue.get();
    {
      std::lock_guard lock(mutex_);
      job_queue_to_transfer_ = std::move(job_queue);
    }
    cv_.notify_one();
    job_queue_ptr->RunUntilStopped();
  }

  std::unique_ptr<JobQueue> TakeJobQueue() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return job_queue_to_transfer_ != nullptr; });
    return std::move(job_queue_to_transfer_);
  }

 private:
  SbThreadPriority priority_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::unique_ptr<JobQueue> job_queue_to_transfer_;
};

std::unique_ptr<JobThread> JobThread::Create(std::string_view thread_name) {
  return JobThread::Create(thread_name, kSbThreadPriorityNormal);
}

std::unique_ptr<JobThread> JobThread::Create(std::string_view thread_name,
                                             SbThreadPriority priority) {
  auto thread = std::make_unique<WorkerThread>(thread_name, priority);
  thread->Start();
  auto job_queue = thread->TakeJobQueue();
  return std::unique_ptr<JobThread>(
      new JobThread(std::move(thread), std::move(job_queue)));
}

JobThread::JobThread(std::unique_ptr<WorkerThread> thread,
                     std::unique_ptr<JobQueue> job_queue)
    : thread_(std::move(thread)), job_queue_(std::move(job_queue)) {}

JobThread::~JobThread() {
  Stop();
}

void JobThread::Stop() {
  SB_CHECK(!job_queue_->BelongsToCurrentThread())
      << "Stop() should not be called from the worker thread itself. This "
         "would result in a deadlock during Join().";

  // Use a mutex to ensure that if multiple threads call Stop() (e.g. one
  // explicitly and one via the destructor), they all wait until the join
  // is actually complete.
  std::lock_guard lock(stop_mutex_);
  if (stopped_) {
    return;
  }
  stopped_ = true;

  job_queue_->StopSoon();
  thread_->Join();
}

}  // namespace starboard
