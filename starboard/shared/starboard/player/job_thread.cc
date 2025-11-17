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

#include <string>

#include "starboard/common/check_op.h"
#include "starboard/common/condition_variable.h"
#include "starboard/shared/pthread/thread_create_priority.h"

#if defined(ANDROID)
#include "starboard/android/shared/jni_state.h"
#endif

namespace starboard::shared::starboard::player {

namespace {

struct ThreadParam {
  explicit ThreadParam(JobThread* job_thread,
                       const char* name,
                       SbThreadPriority priority)
      : condition_variable(mutex),
        job_thread(job_thread),
        thread_name(name),
        thread_priority(priority) {}
  Mutex mutex;
  ConditionVariable condition_variable;
  JobThread* job_thread;
  std::string thread_name;
  SbThreadPriority thread_priority;
};

}  // namespace

JobThread::JobThread(const char* thread_name,
                     int64_t stack_size,
                     SbThreadPriority priority) {
  ThreadParam thread_param(this, thread_name, priority);

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  if (stack_size > 0) {
    pthread_attr_setstacksize(&attributes, stack_size);
  }

  pthread_create(&thread_, &attributes, &JobThread::ThreadEntryPoint,
                 &thread_param);
  pthread_attr_destroy(&attributes);

  SB_DCHECK_NE(thread_, 0);
  ScopedLock scoped_lock(thread_param.mutex);
  while (!job_queue_) {
    thread_param.condition_variable.Wait();
  }
  SB_DCHECK(job_queue_);
}

JobThread::~JobThread() {
  // TODO: There is a potential race condition here since job_queue_ can get
  // reset if it's is stopped while this dtor is running. Thus, avoid stopping
  // job_queue_ before JobThread is destructed.
  if (job_queue_) {
    job_queue_->Schedule(std::bind(&JobQueue::StopSoon, job_queue_.get()));
  }
  pthread_join(thread_, nullptr);
}

// static
void* JobThread::ThreadEntryPoint(void* context) {
  ThreadParam* param = static_cast<ThreadParam*>(context);
  SB_DCHECK(param != nullptr);

  pthread_setname_np(pthread_self(), param->thread_name.c_str());
  shared::pthread::ThreadSetPriority(param->thread_priority);

  JobThread* job_thread = param->job_thread;
  {
    ScopedLock scoped_lock(param->mutex);
    job_thread->job_queue_.reset(new JobQueue);
    param->condition_variable.Signal();
  }
  job_thread->RunLoop();

#if defined(ANDROID)
  android::shared::JNIState::GetVM()->DetachCurrentThread();
#endif
  return nullptr;
}

void JobThread::RunLoop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  job_queue_->RunUntilStopped();
  // TODO: Investigate removing this line to avoid the race condition in the
  // dtor.
  job_queue_.reset();
}

}  // namespace starboard::shared::starboard::player
