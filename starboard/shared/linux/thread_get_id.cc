// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>

#include "starboard/thread.h"

// Copied from
// https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_posix.cc;drc=c1a1260ef7c870242f30982e59a5d98a7f6cdfb9

namespace {

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally.
thread_local SbThreadId g_thread_id = kSbThreadInvalidId;

// A boolean value that indicates that the value stored in |g_thread_id| on the
// main thread is invalid, because it hasn't been updated since the process
// forked.
//
// This used to work by setting |g_thread_id| to -1 in a pthread_atfork handler.
// However, when a multithreaded process forks, it is only allowed to call
// async-signal-safe functions until it calls an exec() syscall. However,
// accessing TLS may allocate (see crbug.com/1275748), which is not
// async-signal-safe and therefore causes deadlocks, corruption, and crashes.
//
// It's Atomic to placate TSAN.
std::atomic<bool> g_main_thread_tid_cache_valid = false;

// Tracks whether the current thread is the main thread, and therefore whether
// |g_main_thread_tid_cache_valid| is relevant for the current thread. This is
// also updated by PlatformThread::CurrentId().
thread_local bool g_is_main_thread = true;

void InvalidateTidCache() {
  g_main_thread_tid_cache_valid.store(false, std::memory_order_relaxed);
}

class InitAtFork {
 public:
  InitAtFork() { pthread_atfork(nullptr, nullptr, InvalidateTidCache); }
};

}  // namespace

// static
SbThreadId SbThreadGetId() {
  static InitAtFork init_at_fork;
  if (g_thread_id == kSbThreadInvalidId ||
      (g_is_main_thread &&
       !g_main_thread_tid_cache_valid.load(std::memory_order_relaxed))) {
    // Update the cached tid.
    g_thread_id = static_cast<SbThreadId>(syscall(SYS_gettid));
    // If this is the main thread, we can mark the tid_cache as valid.
    // Otherwise, stop the current thread from always entering this slow path.
    if (g_thread_id == getpid()) {
      g_main_thread_tid_cache_valid.store(true, std::memory_order_relaxed);
    } else {
      g_is_main_thread = false;
    }
  }
  return g_thread_id;
}
