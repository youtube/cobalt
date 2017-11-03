// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/condition_variable.h"

#include <pthread.h>

#include "starboard/shared/pthread/is_success.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

using starboard::shared::starboard::SetInitialized;

namespace {
struct ConditionVariableAttributes {
 public:
  ConditionVariableAttributes() {
    valid_ = IsSuccess(pthread_condattr_init(&attributes_));
  }
  ~ConditionVariableAttributes() {
    if (valid_) {
      SB_CHECK(IsSuccess(pthread_condattr_destroy(&attributes_)));
    }
  }

  bool valid() const { return valid_; }
  pthread_condattr_t* attributes() { return &attributes_; }

 private:
  bool valid_;
  pthread_condattr_t attributes_;
};
}  // namespace

bool SbConditionVariableCreate(SbConditionVariable* out_condition,
                               SbMutex* /*opt_mutex*/) {
  if (!out_condition) {
    return false;
  }

  ConditionVariableAttributes attributes;
  if (!attributes.valid()) {
    SB_DLOG(ERROR) << "Failed to call pthread_condattr_init().";
    return false;
  }

#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  // Always use CLOCK_MONOTONIC so that SbConditionVariableWaitTimed() will
  // not be based off of the system clock (which can lead to erroneous
  // behavior if the system clock is changed while a process is running).
  if (!IsSuccess(pthread_condattr_setclock(
           attributes.attributes(), CLOCK_MONOTONIC))) {
    SB_DLOG(ERROR) << "Failed to call pthread_condattr_setclock().";
    return false;
  }
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)

  bool status = IsSuccess(pthread_cond_init(
                    &out_condition->condition, attributes.attributes()));

  // We mark that we are initialized regardless of whether initialization
  // was successful or not.
  SetInitialized(&out_condition->initialized_state);

  if (!status) {
    SB_DLOG(ERROR) << "Failed to call pthread_cond_init().";
  }

  return status;
}
