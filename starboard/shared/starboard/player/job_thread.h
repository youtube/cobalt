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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_JOB_THREAD_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_JOB_THREAD_H_

#include <atomic>
#include <memory>
#include <string_view>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/thread.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/job_queue.h"

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

namespace starboard {

// This class implements a thread that holds a JobQueue.
//
// NOTE: It is the caller's responsibility to ensure that the JobThread object
// remains valid while its methods are being called. This class does not
// provide internal synchronization to prevent use-after-free if the object
// is destroyed by one thread while another thread is calling its public
// methods (e.g., Schedule()). Such lifetime management must be handled at the
// caller level.
class JobThread {
 public:
  static std::unique_ptr<JobThread> Create(std::string_view thread_name);
  static std::unique_ptr<JobThread> Create(std::string_view thread_name,
                                           SbThreadPriority priority);
  ~JobThread();

  bool BelongsToCurrentThread() const {
    return job_queue_->BelongsToCurrentThread();
  }

  JobQueue::JobToken Schedule(const JobQueue::Job& job,
                              int64_t delay_usec = 0) {
    return job_queue_->Schedule(job, delay_usec);
  }

  JobQueue::JobToken Schedule(JobQueue::Job&& job, int64_t delay_usec = 0) {
    return job_queue_->Schedule(std::move(job), delay_usec);
  }

  void ScheduleAndWait(const JobQueue::Job& job) {
    job_queue_->ScheduleAndWait(job);
  }

  // TODO: Calling ScheduleAndWait with a call to JobQueue::StopSoon will cause
  // heap-use-after-free errors in ScheduleAndWait due to JobQueue dtor
  // occasionally running before ScheduleAndWait has finished.
  void ScheduleAndWait(JobQueue::Job&& job) {
    job_queue_->ScheduleAndWait(std::move(job));
  }

  void RemoveJobByToken(JobQueue::JobToken job_token) {
    return job_queue_->RemoveJobByToken(job_token);
  }

  JobQueue* job_queue() const { return job_queue_.get(); }

  // Remove any pending tasks and stop scheduling any more tasks.
  // This method returns only after the current running task is completed, if
  // any. This can be called when call sites want to ensure that no pending
  // tasks are running while the owning object is being destroyed.
  // This is useful because tasks might access members of the owning object
  // that could be destroyed before the JobThread member itself is destroyed.
  // For example, a unique_ptr member holding the JobThread is set to nullptr
  // as soon as its destruction begins, causing tasks that access it to see
  // a null pointer even while the JobThread's destructor is still waiting
  // for them to finish. For details, see http://b/477902972#comment2.
  void Stop();

 private:
  class WorkerThread;
  JobThread(std::unique_ptr<WorkerThread> thread,
            std::unique_ptr<JobQueue> job_queue);

  std::mutex stop_mutex_;
  bool stopped_ = false;

  const std::unique_ptr<WorkerThread> thread_;
  const std::unique_ptr<JobQueue> job_queue_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_JOB_THREAD_H_
