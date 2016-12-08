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

#include "base/threading/thread_local.h"

#include "base/logging.h"
#include "starboard/thread.h"

namespace base {

namespace internal {

// static
void ThreadLocalPlatform::AllocateSlot(SlotType& slot) {
  slot = SbThreadCreateLocalKey(NULL);
  CHECK_NE(kSbThreadLocalKeyInvalid, slot);
}

// static
void ThreadLocalPlatform::FreeSlot(SlotType& slot) {
  SbThreadDestroyLocalKey(slot);
}

// static
void* ThreadLocalPlatform::GetValueFromSlot(SlotType& slot) {
  return SbThreadGetLocalValue(slot);
}

// static
void ThreadLocalPlatform::SetValueInSlot(SlotType& slot, void* value) {
  bool result = SbThreadSetLocalValue(slot, value);
  DCHECK(result);
}

}  // namespace internal

}  // namespace base
