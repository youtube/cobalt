// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Provides definitions useful for working with SbOnce implemented using
// SbAtomic32.

#ifndef STARBOARD_SHARED_STARBOARD_ONCE_TYPES_PUBLIC_H_
#define STARBOARD_SHARED_STARBOARD_ONCE_TYPES_PUBLIC_H_

#include "starboard/atomic.h"
#include "starboard/shared/starboard/lazy_initialization_public.h"

// Defines once types for the platform-independent atomics-based implementation
// of SbOnce() defined in shared/once.cc.

// Transparent Once control handle.
typedef InitializedState SbOnceControl;

// Once static initializer.
#define SB_ONCE_INITIALIZER \
  { INITIALIZED_STATE_UNINITIALIZED }

#endif  // STARBOARD_SHARED_STARBOARD_ONCE_TYPES_PUBLIC_H_
