/*
 * Copyright 2018 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "starboard/common/thread.h"

#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <optional>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/thread_platform.h"
#include "starboard/system.h"

namespace starboard {

struct Thread::Data {
  std::string name_;
  pthread_t thread_ = 0;
  std::atomic_bool started_{false};
  std::atomic_bool join_called_{false};
  Semaphore join_sema_;
  int64_t stack_size_;
};

Thread::Thread(const std::string& name, int64_t stack_size) {
  d_.reset(new Thread::Data);
  d_->name_ = name;
  d_->stack_size_ = stack_size;
}

Thread::~Thread() {
  // A started thread must be joined before destruction.
  if (d_->started_.load()) {
    SB_DCHECK(d_->join_called_.load())
        << "Thread '" << d_->name_ << "' was not joined before destruction.";
  }
}

void Thread::Start() {
  SB_DCHECK(!d_->started_.load());
  d_->started_.store(true);

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  if (d_->stack_size_ > 0) {
    pthread_attr_setstacksize(&attributes, d_->stack_size_);
  }

  const int result =
      pthread_create(&d_->thread_, &attributes, ThreadEntryPoint, this);
  pthread_attr_destroy(&attributes);
  SB_CHECK_EQ(result, 0);
}

void Thread::Sleep(int64_t microseconds) {
  usleep(microseconds);
}

void Thread::SleepMilliseconds(int value) {
  return Sleep(static_cast<int64_t>(value) * 1000);
}

bool Thread::WaitForJoin(int64_t timeout) {
  bool joined = d_->join_sema_.TakeWait(timeout);
  if (joined) {
    SB_DCHECK(d_->join_called_.load());
  }
  return joined;
}

starboard::Semaphore* Thread::join_sema() {
  return &d_->join_sema_;
}

std::atomic_bool* Thread::joined_bool() {
  return &d_->join_called_;
}

void* Thread::ThreadEntryPoint(void* context) {
  Thread* this_ptr = static_cast<Thread*>(context);
#if defined(__APPLE__)
  pthread_setname_np(this_ptr->d_->name_.c_str());
#else
  pthread_setname_np(pthread_self(), this_ptr->d_->name_.c_str());
#endif
  this_ptr->Run();

  TerminateOnThread();
  return NULL;
}

void Thread::Join() {
  SB_DCHECK_EQ(d_->join_called_.load(), false);

  d_->join_called_.store(true);
  d_->join_sema_.Put();

  if (!d_->started_.load()) {
    SB_LOG(WARNING) << "Join() called on thread '" << d_->name_
                    << "' which was not started. Ignoring.";
    return;
  }

  int result = pthread_join(d_->thread_, /*retval=*/nullptr);
  if (result != 0) {
    char error_msg[256];
    SbSystemGetErrorString(static_cast<SbSystemError>(result), error_msg,
                           sizeof(error_msg));
    SB_CHECK_EQ(result, 0) << "Could not join thread: " << error_msg;
  }
}

bool Thread::join_called() const {
  return d_->join_called_.load();
}

}  // namespace starboard
