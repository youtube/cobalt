// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

struct ThreadParam {
  explicit ThreadParam(JobThread* job_thread)
      : condition_variable(mutex), job_thread(job_thread) {}
  Mutex mutex;
  ConditionVariable condition_variable;
  JobThread* job_thread;
};

}  // namespace

JobThread::JobThread(const char* thread_name) {
  ThreadParam thread_param(this);
  thread_ =
      SbThreadCreate(0, kSbThreadPriorityNormal, kSbThreadNoAffinity, true,
                     thread_name, &JobThread::ThreadEntryPoint, &thread_param);
  SB_DCHECK(SbThreadIsValid(thread_));
  ScopedLock scoped_lock(thread_param.mutex);
  while (!job_queue_) {
    thread_param.condition_variable.Wait();
  }
  SB_DCHECK(job_queue_);
}

JobThread::~JobThread() {
  job_queue_->Schedule(std::bind(&JobQueue::StopSoon, job_queue_.get()));
  SbThreadJoin(thread_, NULL);
}

// static
void* JobThread::ThreadEntryPoint(void* context) {
  ThreadParam* param = static_cast<ThreadParam*>(context);
  SB_DCHECK(param != NULL);
  JobThread* job_thread = param->job_thread;
  {
    ScopedLock scoped_lock(param->mutex);
    job_thread->job_queue_.reset(new JobQueue);
    param->condition_variable.Signal();
  }
  job_thread->RunLoop();
  return NULL;
}

void JobThread::RunLoop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  job_queue_->RunUntilStopped();
  job_queue_.reset();
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
