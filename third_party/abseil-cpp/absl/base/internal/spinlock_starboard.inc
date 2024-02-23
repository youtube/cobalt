// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is a Starboard-specific part of spinlock_wait.cc

#if defined(STARBOARD)
#include "starboard/thread.h"

#include <atomic>
#include <ctime>

#include "absl/base/internal/errno_saver.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/port.h"

extern "C" {

ABSL_ATTRIBUTE_WEAK void ABSL_INTERNAL_C_SYMBOL(AbslInternalSpinLockDelay)(
    std::atomic<uint32_t>* /* lock_word */, uint32_t /* value */, int loop,
    absl::base_internal::SchedulingMode /* mode */) {
  absl::base_internal::ErrnoSaver errno_saver;
  if (loop == 0) {
  } else if (loop == 1) {
    SbThreadYield();
  } else {
    SbThreadSleep(loop);
  }
}

ABSL_ATTRIBUTE_WEAK void ABSL_INTERNAL_C_SYMBOL(AbslInternalSpinLockWake)(
    std::atomic<uint32_t>* /* lock_word */, bool /* all */) {}

}  // extern "C"

#endif  // defined(STARBOARD)
