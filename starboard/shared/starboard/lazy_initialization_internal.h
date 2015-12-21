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
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/lazy_initialization_public.h"

namespace starboard {
namespace shared {
namespace starboard {

// The utility functions defined here use atomics and spin-locks to allow for
// easy lazy initialization in a thread-safe way.

// Returns false if initialization is necessary, otherwise returns true.
// If false is returned, you must initialize the state (e.g. by eventually
// calling SetInitialized() or else other threads waiting for initialization
// to complete will wait forever.)
bool EnsureInitialized(InitializedState* state);

// Returns true if the state is initialized, false otherwise.  Do not
// use the outcome of this function to make a decision on whether to initialize
// or not, use EnsureInitialized() for that.
bool IsInitialized(InitializedState* state);

// Sets the state as being initialized.
void SetInitialized(InitializedState* state);

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_INTERNAL_H_
