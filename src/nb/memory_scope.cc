/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "nb/memory_scope.h"
#include "starboard/atomic.h"
#include "starboard/common/log.h"

NbMemoryScopeReporter* s_memory_reporter_ = NULL;

bool NbSetMemoryScopeReporter(NbMemoryScopeReporter* reporter) {
  // Flush all the pending memory writes out to main memory so that
  // other threads see a fully constructed reporter.
  SbAtomicMemoryBarrier();
  s_memory_reporter_ = reporter;
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  SbLogRaw("\nMemory Scope Reporting is disabled because this build does "
           "not support it. Try a QA, devel or debug build.\n");
  return false;
#else
  return true;
#endif
}

void NbPushMemoryScope(NbMemoryScopeInfo* memory_scope_info) {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter_)) {
    return;
  }
  s_memory_reporter_->push_memory_scope_cb(
    s_memory_reporter_->context,
    memory_scope_info);
#endif
}

void NbPopMemoryScope() {
#if !defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  return;
#else
  if (SB_LIKELY(!s_memory_reporter_)) {
    return;
  }
  s_memory_reporter_->pop_memory_scope_cb(s_memory_reporter_->context);
#endif
}
