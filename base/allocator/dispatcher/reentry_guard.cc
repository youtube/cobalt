// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/reentry_guard.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#include <pthread.h>
#endif

namespace base::allocator::dispatcher {

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
// pthread_key_t has different signedness on Mac and Android. Store the null
// value in a strongly-typed constant to avoid "comparison of integers of
// different signs" warnings when comparing with 0.
constexpr pthread_key_t kNullKey = 0;

pthread_key_t ReentryGuard::entered_key_ = kNullKey;

void ReentryGuard::InitTLSSlot() {
  if (entered_key_ == kNullKey) {
    int error = pthread_key_create(&entered_key_, nullptr);
    CHECK(!error);
    // Touch the TLS slot immediately to force any allocations.
    // TODO(crbug.com/40062835): Use this technique to avoid allocations
    // in PoissonAllocationSampler::ScopedMuteThreadSamples, which will make
    // ReentryGuard redundant.
    pthread_setspecific(entered_key_, nullptr);
  }

  DCHECK_NE(entered_key_, kNullKey);
}

#else

void ReentryGuard::InitTLSSlot() {}

#endif

void ReentryGuard::RecordTLSSlotToCrashKey() {}

}  // namespace base::allocator::dispatcher
