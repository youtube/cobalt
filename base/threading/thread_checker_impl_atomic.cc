/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/threading/thread_checker_impl.h"

#include "base/threading/platform_thread.h"

namespace base {
namespace {

inline base::subtle::Atomic32 CurrentThreadIdAsAtomic() {
  return static_cast<base::subtle::Atomic32>(PlatformThread::CurrentId());
}

const base::subtle::Atomic32 kInvalidThreadIdAtomic =
    static_cast<base::subtle::Atomic32>(kInvalidThreadId);

}  // namespace

ThreadCheckerImpl::ThreadCheckerImpl()
    : valid_thread_id_(kInvalidThreadIdAtomic) {
  EnsureThreadIdAssigned();
}

ThreadCheckerImpl::~ThreadCheckerImpl() {
}

bool ThreadCheckerImpl::CalledOnValidThread() const {
  EnsureThreadIdAssigned();
  base::subtle::Atomic32 current_thread =
      base::subtle::NoBarrier_Load(&valid_thread_id_);
  return current_thread == CurrentThreadIdAsAtomic();
}

void ThreadCheckerImpl::DetachFromThread() {
  base::subtle::NoBarrier_Store(&valid_thread_id_, kInvalidThreadIdAtomic);
}

void ThreadCheckerImpl::EnsureThreadIdAssigned() const {
  // Set valid_thread_id_ to the current thread id if valid_thread_id_ is
  // currently unset (that is, set to kInvalidThreadIdAtomic).
  base::subtle::NoBarrier_CompareAndSwap(
      &valid_thread_id_,
      kInvalidThreadIdAtomic,
      CurrentThreadIdAsAtomic());
}

}  // namespace base
