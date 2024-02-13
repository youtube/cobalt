// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/thread-id.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {

namespace {

#if defined(V8_OS_STARBOARD)
static_assert(sizeof(int) <= sizeof(void*), "int won't fit in void*");
DEFINE_LAZY_LEAKY_OBJECT_GETTER(base::Thread::LocalStorageKey, GetThreadIdKey,
                                base::Thread::CreateThreadLocalKey())
#else
thread_local int thread_id = 0;
#endif  // defined(V8_OS_STARBOARD)

std::atomic<int> next_thread_id{1};

}  // namespace

// static
ThreadId ThreadId::TryGetCurrent() {
#if defined(V8_OS_STARBOARD)
  int thread_id = static_cast<int>(reinterpret_cast<intptr_t>(
      base::Thread::GetThreadLocal(*GetThreadIdKey())));
#endif  // defined(V8_OS_STARBOARD)
  return thread_id == 0 ? Invalid() : ThreadId(thread_id);
}

// static
int ThreadId::GetCurrentThreadId() {
#if defined(V8_OS_STARBOARD)
  auto key = *GetThreadIdKey();
  int thread_id = static_cast<int>(reinterpret_cast<intptr_t>(
      base::Thread::GetThreadLocal(*GetThreadIdKey())));
#endif  // defined(V8_OS_STARBOARD)
  if (thread_id == 0) {
    thread_id = next_thread_id.fetch_add(1);
    CHECK_LE(1, thread_id);
#if defined(V8_OS_STARBOARD)
    base::Thread::SetThreadLocal(
        key, reinterpret_cast<void*>(static_cast<intptr_t>(thread_id)));
#endif  // defined(V8_OS_STARBOARD)
  }
  return thread_id;
}

}  // namespace internal
}  // namespace v8
