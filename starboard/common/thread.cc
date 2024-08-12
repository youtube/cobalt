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

#include "starboard/common/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/semaphore.h"

namespace starboard {

struct Thread::Data {
  std::string name_;
  pthread_t thread_ = 0;
  atomic_bool started_;
  atomic_bool join_called_;
  Semaphore join_sema_;
};

Thread::Thread(const std::string& name) {
  d_.reset(new Thread::Data);
  d_->name_ = name;
}

Thread::~Thread() {
  SB_DCHECK(d_->join_called_.load()) << "Join not called on thread.";
}

void Thread::Start() {
  SB_DCHECK(!d_->started_.load());
  d_->started_.store(true);

  pthread_create(&d_->thread_, NULL, ThreadEntryPoint, this);

  // pthread_create() above produced an invalid thread handle.
  SB_DCHECK(d_->thread_ != 0);
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

starboard::atomic_bool* Thread::joined_bool() {
  return &d_->join_called_;
}

void* Thread::ThreadEntryPoint(void* context) {
  Thread* this_ptr = static_cast<Thread*>(context);
  pthread_setname_np(pthread_self(), this_ptr->d_->name_.c_str());
  this_ptr->Run();
  return NULL;
}

void Thread::Join() {
  SB_DCHECK(d_->join_called_.load() == false);

  d_->join_called_.store(true);
  d_->join_sema_.Put();

  if (pthread_join(d_->thread_, NULL) != 0) {
    SB_DCHECK(false) << "Could not join thread.";
  }
}

bool Thread::join_called() const {
  return d_->join_called_.load();
}

}  // namespace starboard
