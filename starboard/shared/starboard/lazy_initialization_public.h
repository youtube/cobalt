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

#ifndef STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_PUBLIC_H_
#define STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_PUBLIC_H_

#include "starboard/types.h"

// The utility functions defined here use atomics and spin-locks to allow for
// easy lazy initialization in a thread-safe way.

// An InitializedState should be treated as an opaque type that can be
// initialized to kInitializedStateUninitialized and then passed into the
// functions in this file to transition it first to a "initializing" state and
// then an "initialized" state.  Note that this is intended to be used as a
// SbAtomic32, however due to technical reasons we can't include
// "starboard/atomic.h".
typedef int32_t InitializedState;
#define INITIALIZED_STATE_UNINITIALIZED 1

#endif  // STARBOARD_SHARED_STARBOARD_LAZY_INITIALIZATION_PUBLIC_H_
