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

#include <memory>
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
class JobThread {
 public:
  explicit JobThread(const char* thread_name,
                     int64_t stack_size = 0,
                     SbThreadPriority priority = kSbThreadPriorityNormal);
  ~JobThread();

  bool BelongsToCurrentThread() const {
    SB_DCHECK(job_queue_);

    return job_queue_->BelongsToCurrentThread();
  }

  JobQueue::JobToken Schedule(const JobQueue::Job& job,
                              int64_t delay_usec = 0) {
    SB_DCHECK(job_queue_);

    return job_queue_->Schedule(job, delay_usec);
  }

  JobQueue::JobToken Schedule(JobQueue::Job&& job, int64_t delay_usec = 0) {
    SB_DCHECK(job_queue_);

    return job_queue_->Schedule(std::move(job), delay_usec);
  }

  void ScheduleAndWait(const JobQueue::Job& job) {
    SB_DCHECK(job_queue_);

    job_queue_->ScheduleAndWait(job);
  }

  // TODO: Calling ScheduleAndWait with a call to JobQueue::StopSoon will cause
  // heap-use-after-free errors in ScheduleAndWait due to JobQueue dtor
  // occasionally running before ScheduleAndWait has finished.
  void ScheduleAndWait(JobQueue::Job&& job) {
    SB_DCHECK(job_queue_);

    job_queue_->ScheduleAndWait(std::move(job));
  }

  void RemoveJobByToken(JobQueue::JobToken job_token) {
    SB_DCHECK(job_queue_);

    return job_queue_->RemoveJobByToken(job_token);
  }

 private:
  class WorkerThread;

  void RunLoop();

  std::unique_ptr<Thread> thread_;
  std::unique_ptr<JobQueue> job_queue_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_JOB_THREAD_H_
