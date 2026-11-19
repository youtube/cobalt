// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: Common functions for NACL and all Linux and
// Linux-derivatives.

#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if !defined(THREAD_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
    BUILDFLAG(IS_COBALT) && BUILDFLAG(IS_STARBOARD) &&           \
    defined(ARCH_CPU_32_BITS)
#include "base/feature_list.h"
#include "base/features.h"
#endif

namespace base {

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(THREAD_SANITIZER) && !defined(ADDRESS_SANITIZER) && \
    BUILDFLAG(IS_COBALT) && BUILDFLAG(IS_STARBOARD) &&           \
    defined(ARCH_CPU_32_BITS)
  if (FeatureList::GetInstance() &&
      FeatureList::IsEnabled(features::kCobaltReduceStarboardThreadStackSize)) {
    // Reduces default helper thread stacks to 256KB to save virtual memory on
    // 32-bit Starboard devices (b/552601001). High-risk threads (renderer, GPU,
    // Blink worker threads) are explicitly assigned 1MB elsewhere.
    return 256 * 1024;
  }
#endif
#if !defined(THREAD_SANITIZER) && defined(__GLIBC__)
  // Generally glibc sets ample default stack sizes, so use the default there.
  return 0;
#elif !defined(THREAD_SANITIZER)
  // Other libcs (uclibc, musl, etc) tend to use smaller stacks, often too small
  // for chromium. Make sure we have enough space to work with here. Note that
  // for comparison glibc stacks are generally around 8MB.
  return 2 * (1 << 20);
#else
  // ThreadSanitizer bloats the stack heavily. Evidence has been that the
  // default stack size isn't enough for some browser tests.
  return 2 * (1 << 23);  // 2 times 8192K (the default stack size on Linux).
#endif
}

void InitThreading() {}

void TerminateOnThread() {}

}  // namespace base
