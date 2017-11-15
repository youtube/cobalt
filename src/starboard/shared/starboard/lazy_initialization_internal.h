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

#ifndef STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_INTERNAL_H_

#include "starboard/atomic.h"
#include "starboard/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/lazy_initialization_public.h"
#include "starboard/thread.h"

// INITIALIZED_STATE_UNINITIALIZED is defined in the header.
#define INITIALIZED_STATE_INITIALIZING 2
#define INITIALIZED_STATE_INITIALIZED 3

namespace starboard {
namespace shared {
namespace starboard {

// The utility functions defined here use atomics and spin-locks to allow for
// easy lazy initialization in a thread-safe way.

// Returns false if initialization is necessary, otherwise returns true.
// If false is returned, you must initialize the state (e.g. by eventually
// calling SetInitialized() or else other threads waiting for initialization
// to complete will wait forever.)
static SB_C_INLINE bool EnsureInitialized(InitializedState* state) {
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

// Returns true if the state is initialized, false otherwise.  Do not
// use the outcome of this function to make a decision on whether to initialize
// or not, use EnsureInitialized() for that.
static SB_C_INLINE bool IsInitialized(InitializedState* state) {
  return SbAtomicNoBarrier_Load(state) == INITIALIZED_STATE_INITIALIZED;
}

// Sets the state as being initialized.
static SB_C_INLINE void SetInitialized(InitializedState* state) {
  // Mark that we are initialized now.
  SbAtomicRelease_Store(state, INITIALIZED_STATE_INITIALIZED);
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_INTERNAL_H_
