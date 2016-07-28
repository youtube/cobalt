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

#include "starboard/shared/starboard/lazy_initialization_internal.h"

#include "starboard/log.h"
#include "starboard/thread.h"

// INITIALIZED_STATE_UNINITIALIZED is defined in the header.
#define INITIALIZED_STATE_INITIALIZING 2
#define INITIALIZED_STATE_INITIALIZED 3

namespace starboard {
namespace shared {
namespace starboard {

bool EnsureInitialized(InitializedState* state) {
  // Check what state we're in, and if we find that we are uninitialized,
  // simultaneously mark us as initializing and return to the caller.
  InitializedState original = SbAtomicNoBarrier_CompareAndSwap(
      state, INITIALIZED_STATE_UNINITIALIZED, INITIALIZED_STATE_INITIALIZING);
  if (original == INITIALIZED_STATE_UNINITIALIZED) {
    // If we were uninitialized, we are now marked as initializing and so
    // we relay this information to the caller, so that they may initialize.
    return false;
  } else if (original == INITIALIZED_STATE_INITIALIZING) {
    // If the current state is that we are being initialized, spin until
    // initialization is complete, then return.
    do {
      SbThreadYield();
    } while (SbAtomicAcquire_Load(state) != INITIALIZED_STATE_INITIALIZED);
  } else {
    SB_DCHECK(original == INITIALIZED_STATE_INITIALIZED);
  }

  return true;
}

bool IsInitialized(InitializedState* state) {
  return SbAtomicNoBarrier_Load(state) == INITIALIZED_STATE_INITIALIZED;
}

void SetInitialized(InitializedState* state) {
  // Mark that we are initialized now.
  SbAtomicRelease_Store(state, INITIALIZED_STATE_INITIALIZED);
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
