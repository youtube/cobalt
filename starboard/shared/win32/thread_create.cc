// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/thread.h"

#include <process.h>
#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace sbwin32 = starboard::shared::win32;

using sbwin32::DebugLogWinError;
using sbwin32::GetThreadSubsystemSingleton;
using sbwin32::SbThreadPrivate;
using sbwin32::ThreadCreateInfo;
using sbwin32::ThreadSubsystemSingleton;
using sbwin32::wchar_tToUTF8;

void ResetWinError() {
  SetLastError(0);
}

int RunThreadLocalDestructors(ThreadSubsystemSingleton* singleton) {
  int num_destructors_called = 0;

  for (auto it = singleton->thread_local_keys_.begin();
       it != singleton->thread_local_keys_.end();) {
    auto curr_it = it++;

    if (!curr_it->second->destructor) {
      continue;
    }
    auto key = curr_it->second;
    void* entry = SbThreadGetLocalValue(key);
    if (!entry) {
      continue;
    }
    SbThreadSetLocalValue(key, nullptr);
    ++num_destructors_called;
    curr_it->second->destructor(entry);
  }
  return num_destructors_called;
}

int CountTlsObjectsRemaining(ThreadSubsystemSingleton* singleton) {
  int num_objects_remain = 0;
  for (auto it = singleton->thread_local_keys_.begin();
       it != singleton->thread_local_keys_.end(); ++it) {
    if (!it->second->destructor) {
      continue;
    }
    auto key = it->second;
    void* entry = SbThreadGetLocalValue(key);
    if (!entry) {
      continue;
    }
    ++num_objects_remain;
  }
  return num_objects_remain;
}

void CallThreadLocalDestructorsMultipleTimes() {
  // The number of passes conforms to the base_unittests. This is useful for
  // destructors that insert new objects into thread local storage. These
  // objects then need to be destroyed as well in subsequent passes. The total
  // number of passes is 4, which is one more than what base_unittest tests
  // for.
  const int kNumDestructorPasses = 4;

  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();
  int num_tls_objects_remaining = 0;
  // TODO note that the implementation below holds a global lock
  // while processing TLS destructors on thread exit. This could
  // be a bottleneck in some scenarios. A lockless approach may be preferable.
  SbMutexAcquire(&singleton->mutex_);

  for (int i = 0; i < kNumDestructorPasses; ++i) {
    // Run through each destructor and call it.
    const int num_destructors_called = RunThreadLocalDestructors(singleton);
    if (0 == num_destructors_called) {
      break;  // No more destructors to call.
    }
  }
  num_tls_objects_remaining = CountTlsObjectsRemaining(singleton);
  SbMutexRelease(&singleton->mutex_);

  SB_DCHECK(num_tls_objects_remaining == 0) << "Dangling objects in TLS exist.";
}

namespace {

unsigned ThreadTrampoline(void* thread_create_info_context) {
  std::unique_ptr<ThreadCreateInfo> info(
      static_cast<ThreadCreateInfo*>(thread_create_info_context));

  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();
  SbThreadSetLocalValue(singleton->thread_private_key_, &info->thread_private_);
  SbThreadSetName(info->name_.c_str());

  void* result = info->entry_point_(info->user_context_);

  CallThreadLocalDestructorsMultipleTimes();

  SbMutexAcquire(&info->thread_private_.mutex_);
  info->thread_private_.result_ = result;
  info->thread_private_.result_is_valid_ = true;
  SbConditionVariableSignal(&info->thread_private_.condition_);
  while (info->thread_private_.wait_for_join_) {
    SbConditionVariableWait(&info->thread_private_.condition_,
                            &info->thread_private_.mutex_);
  }
  SbMutexRelease(&info->thread_private_.mutex_);

  return 0;
}

int SbThreadPriorityToWin32Priority(SbThreadPriority priority) {
  switch (priority) {
    case kSbThreadPriorityLowest:
      return THREAD_PRIORITY_LOWEST;
    case kSbThreadPriorityLow:
      return THREAD_PRIORITY_BELOW_NORMAL;
    case kSbThreadPriorityNormal:
    case kSbThreadNoPriority:
      return THREAD_PRIORITY_NORMAL;
    case kSbThreadPriorityHigh:
      return THREAD_PRIORITY_ABOVE_NORMAL;
    case kSbThreadPriorityHighest:
      return THREAD_PRIORITY_HIGHEST;
    case kSbThreadPriorityRealTime:
      return THREAD_PRIORITY_TIME_CRITICAL;
  }
  SB_NOTREACHED() << "Invalid priority " << priority;
  return 0;
}
}  // namespace

// Note that SetThreadAffinityMask() is not available on some
// platforms (eg UWP). If it's necessary for a non-UWP platform,
// please fork this implementation for UWP.
SbThread SbThreadCreate(int64_t stack_size,
                        SbThreadPriority priority,
                        SbThreadAffinity affinity,
                        bool joinable,
                        const char* name,
                        SbThreadEntryPoint entry_point,
                        void* context) {
  if (entry_point == NULL) {
    return kSbThreadInvalid;
  }
  ThreadCreateInfo* info = new ThreadCreateInfo();

  info->entry_point_ = entry_point;
  info->user_context_ = context;
  info->thread_private_.wait_for_join_ = joinable;
  if (name) {
    info->name_ = name;
  }

  // Create the thread suspended, and then resume once ThreadCreateInfo::handle_
  // has been set, so that it's always valid in the ThreadCreateInfo
  // destructor.
  uintptr_t handle =
      _beginthreadex(NULL, static_cast<unsigned int>(stack_size),
                     ThreadTrampoline, info, CREATE_SUSPENDED, NULL);
  SB_DCHECK(handle);
  info->thread_private_.handle_ = reinterpret_cast<HANDLE>(handle);
  ResetWinError();
  if (priority != kSbThreadNoPriority &&
      !SetThreadPriority(info->thread_private_.handle_,
                         SbThreadPriorityToWin32Priority(priority)) &&
      !GetLastError()) {
    SB_LOG(ERROR) << "Failed to set priority for thread " << (name ? name : "")
                  << " to " << priority;
    DebugLogWinError();
  }

  ResumeThread(info->thread_private_.handle_);

  return &info->thread_private_;
}
