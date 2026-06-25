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
#include <sys/resource.h>
#include <unistd.h>

#include <atomic>
#include <optional>
#include <string>
#include <string_view>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/thread_platform.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/system.h"

namespace starboard {

namespace {

#if defined(__APPLE__)
// Returns a value between 0 and 1 that is used by SetThreadPriority() to scale
// the desired thread priority between what sched_get_priority_min() and
// sched_get_priority_max() return.
float ThreadPriorityToRelativeSchedPriority(ThreadPriority priority) {
  switch (priority) {
    case ThreadPriority::kLowest:
      return 0.1f;
    case ThreadPriority::kLow:
      return 0.3f;
    case ThreadPriority::kNoPriority:
    case ThreadPriority::kNormal:
      return 0.5f;
    case ThreadPriority::kHigh:
      return 0.7f;
    case ThreadPriority::kHighest:
      return 0.9f;
    case ThreadPriority::kRealTime:
      return 0.99f;
  }
  SB_NOTREACHED();
}
#endif  // defined(__APPLE__)

// Wrapper for changing a thread's priority. On Linux kernel-based platforms
// (including Android), this code relies on setpriority(2), which on Linux
// deviates from the standard and changes per-thread attributes (rather than
// per-process).
//
// On tvOS, use pthread_setschedparam() by translating ThreadPriority into an
// integer between what sched_get_priority_min() and sched_get_priority_max()
// return.
bool SetThreadPriority(ThreadPriority priority) {
#if defined(__APPLE__)
  const float relative_priority =
      ThreadPriorityToRelativeSchedPriority(priority);

  int policy;
  struct sched_param param;

  // Get the current thread scheduling parameters. Only the thread priority
  // will be changed.
  SB_CHECK_EQ(pthread_getschedparam(pthread_self(), &policy, &param), 0);
  const int min_priority = sched_get_priority_min(policy);
  const int max_priority = sched_get_priority_max(policy);

  // Thread priority set with pthread does not appear to have the same range
  // as NSThread priorities. A pthread set to sched_get_priority_max() won't
  // have NSThread.threadPriority == 1.0. Consider switching to NSThread if a
  // higher priority is required.
  param.sched_priority = static_cast<int>(
      min_priority + relative_priority * (max_priority - min_priority));
  return (pthread_setschedparam(pthread_self(), policy, &param) == 0);
#else
  // setpriority returns 0 on success and -1 on failure. The default nice value
  // is 0. See https://linux.die.net/man/2/setpriority
  return (setpriority(PRIO_PROCESS, 0, ThreadPriorityToNiceValue(priority)) ==
          0);
#endif  // defined(__APPLE__)
}

std::optional<size_t> GetOverriddenStackSize() {
  if (features::FeatureList::IsEnabled(
          features::kReduceStarboardThreadStackSize)) {
    return 256 * 1024;
  }
  return std::nullopt;
}

}  // namespace

int ThreadPriorityToNiceValue(ThreadPriority priority) {
  // Nice value settings are shared between Android and Linux.
  // They are selected from looking at:
  // https://android.googlesource.com/platform/frameworks/native/+/jb-dev/include/utils/ThreadDefs.h#35
  switch (priority) {
    case ThreadPriority::kLowest:
      return 19;
    case ThreadPriority::kLow:
      return 10;
    case ThreadPriority::kNoPriority:
    case ThreadPriority::kNormal:
      return 0;
    case ThreadPriority::kHigh:
      return -8;
    case ThreadPriority::kHighest:
      return -16;
    case ThreadPriority::kRealTime:
      return -19;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

struct Thread::Data {
  pthread_t thread_ = 0;
  std::atomic_bool started_{false};
  std::atomic_bool join_called_{false};
  Semaphore join_sema_;
};

Thread::Thread(std::string_view name, const ThreadOptions& options)
    : name_(name),
      priority_(options.priority),
      stack_size_(options.stack_size > 0
                      ? std::make_optional(options.stack_size)
                      : GetOverriddenStackSize()),
      d_(std::make_unique<Data>()) {}

Thread::~Thread() {
  // A started thread must be joined before destruction.
  if (d_->started_.load()) {
    SB_DCHECK(d_->join_called_.load())
        << "Thread '" << name_ << "' was not joined before destruction.";
  }
}

void Thread::Start() {
  SB_DCHECK(!d_->started_.load());
  d_->started_.store(true);

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  if (stack_size_) {
    int err = pthread_attr_setstacksize(&attributes, *stack_size_);
    if (err != 0) {
      SB_LOG(WARNING) << "Failed to set stack size to " << *stack_size_
                      << ", error: " << err << ". Falling back to default.";
    }
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
  pthread_setname_np(this_ptr->name_.c_str());
#else
  pthread_setname_np(pthread_self(), this_ptr->name_.c_str());
#endif
  bool priority_set = false;
  if (this_ptr->priority_) {
    priority_set = SetThreadPriority(*this_ptr->priority_);
    if (!priority_set) {
      SB_LOG(WARNING) << "Failed to set thread priority (unsupported on this "
                         "platform): requested_priority="
                      << static_cast<int>(*this_ptr->priority_);
    }
  }
  SB_LOG(INFO) << "Thread started: name=" << this_ptr->name_ << ", priority="
               << (this_ptr->priority_ && priority_set
                       ? std::to_string(static_cast<int>(*this_ptr->priority_))
                       : "(default)");

  this_ptr->Run();

  TerminateOnThread();
  return nullptr;
}

void Thread::Join() {
  SB_DCHECK_EQ(d_->join_called_.load(), false);

  d_->join_called_.store(true);
  d_->join_sema_.Put();

  if (!d_->started_.load()) {
    SB_LOG(WARNING) << "Join() called on thread '" << name_
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
