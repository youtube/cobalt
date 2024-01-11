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

#include "starboard/common/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/semaphore.h"

namespace starboard {

struct Thread::Data {
  std::string name_;
  SbThread thread_ = kSbThreadInvalid;
  atomic_bool started_;
  atomic_bool join_called_;
  Semaphore join_sema_;
  optional<Thread::Options> options_;
};

Thread::Options::Options()
    : stack_size(0),  // Signal for default stack size.
      priority_(kSbThreadNoPriority),
      joinable(true) {}

Thread::Thread(const std::string& name) {
  d_.reset(new Thread::Data);
  d_->name_ = name;
}

Thread::~Thread() {
  SB_DCHECK(d_->join_called_.load()) << "Join not called on thread.";
}

void Thread::Start(const Options& options) {
  SbThreadEntryPoint entry_point = ThreadEntryPoint;

  SB_DCHECK(!d_->started_.load());
  SB_DCHECK(!d_->options_.has_engaged());
  d_->started_.store(true);
  d_->options_ = options;

  d_->thread_ =
      SbThreadCreate(options.stack_size, options.priority_,
                     kSbThreadNoAffinity,  // default affinity.
                     options.joinable, d_->name_.c_str(), entry_point, this);

  // SbThreadCreate() above produced an invalid thread handle.
  SB_DCHECK(d_->thread_ != kSbThreadInvalid);
}

void Thread::Sleep(int64_t microseconds) {
  SbThreadSleep(microseconds);
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
  this_ptr->Run();
  return NULL;
}

void Thread::Join() {
  SB_DCHECK(d_->join_called_.load() == false);
  SB_DCHECK(d_->options_->joinable) << "Detached thread should not be joined.";

  d_->join_called_.store(true);
  d_->join_sema_.Put();

  if (!SbThreadJoin(d_->thread_, NULL)) {
    SB_DCHECK(false) << "Could not join thread.";
  }
}

bool Thread::join_called() const {
  return d_->join_called_.load();
}

}  // namespace starboard
