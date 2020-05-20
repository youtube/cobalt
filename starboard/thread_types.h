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

// Module Overview: Starboard Thread Types module
//
// Defines platform-specific threading and synchronization primitive types and
// initializers. We hide, but pass through, the platform's primitives to avoid
// doing a lot of work to replicate initialization-less synchronization
// primitives.

// TODO: Implement a cross-platform initless synchronization model, and
// then properly hide thread primitives. This could look like implementing our
// own high-performance init-less lock (e.g. MCS or CLH) using the Atomics
// abstraction, and use that on all platforms and hope really hard that we
// didn't mess it up and then use that to auto-init uninitialized static
// objects.

#ifndef STARBOARD_THREAD_TYPES_H_
#define STARBOARD_THREAD_TYPES_H_

#include "starboard/configuration.h"

#if SB_API_VERSION < 12

// This platform-specific include file must define:
//
//   SbConditionVariable
//   SB_CONDITION_VARIABLE_INITIALIZER
//
//   SbMutex
//   SB_MUTEX_INITIALIZER
//
//   SbThread
//   SbThreadIsValid()
//   kSbThreadInvalid
//
//   SbOnce
//   SB_ONCE_INITIALIIER

#include STARBOARD_THREAD_TYPES_INCLUDE

#ifndef SB_CONDITION_VARIABLE_INITIALIZER
#error "SB_CONDITION_VARIABLE_INITIALIZER not defined by platform."
#endif

#ifndef SB_MUTEX_INITIALIZER
#error "SB_MUTEX_INITIALIZER not defined by platform."
#endif

#ifndef SB_ONCE_INITIALIZER
#error "SB_ONCE_INITIALIZER not defined by platform."
#endif

#endif  // SB_API_VERSION < 12
#endif  // STARBOARD_THREAD_TYPES_H_
