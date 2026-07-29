// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_GETTID_H_
#define STARBOARD_COMMON_GETTID_H_

#include <unistd.h>

#include "build/build_config.h"

// Fallback for platforms where the toolchain/sysroot <unistd.h> does not
// declare gettid() natively (e.g., glibc < 2.30).
#if !defined(gettid)

// TODO(b/517239886): Apple is not a Starboard platform and the Apple
// dependencies on this file need to be cleaned up.
#if BUILDFLAG(IS_IOS_TVOS)
#include <pthread.h>
inline pid_t gettid() {
  return pthread_mach_thread_np(pthread_self());
}

#else  // !BUILDFLAG(IS_IOS_TVOS)

#include <sys/syscall.h>

// Some toolchains/sysroots (like Musl or embedded RDK toolchains) define the
// system call number using the kernel-level prefix __NR_gettid rather than the
// user-level SYS_gettid. Ensure SYS_gettid is defined for compatibility.
#if !defined(SYS_gettid) && defined(__NR_gettid)
#define SYS_gettid __NR_gettid
#endif

#if defined(SYS_gettid)
// Direct syscall fallback because the system's <unistd.h> doesn't define
// gettid().
#define gettid() syscall(SYS_gettid)
#else
#error \
    "gettid() fallback requires SYS_gettid or __NR_gettid to be defined in <sys/syscall.h>. Please ensure your platform sysroot headers define either of these macros, or natively implement gettid() in <unistd.h>."
#endif  // defined(SYS_gettid)

#endif  // !BUILDFLAG(IS_IOS_TVOS)

#endif  // !defined(gettid)

#endif  // STARBOARD_COMMON_GETTID_H_
