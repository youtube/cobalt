// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <unistd.h>

#include "starboard/thread.h"

SbThreadId SbThreadGetId() {
  // Copied from
  // https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/threading/platform_thread_posix.cc;l=115-120;drc=c1a1260ef7c870242f30982e59a5d98a7f6cdfb9
  //
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return static_cast<SbThreadId>(gettid());
}
