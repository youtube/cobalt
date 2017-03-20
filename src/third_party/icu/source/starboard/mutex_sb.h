// Copyright 2016 Google Inc. All Rights Reserved.
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

// Defines a set of atomic integer operations that can be used as lightweight
// synchronization or as building blocks for heavier synchronization
// primitives. Their use is very subtle and requires detailed understanding of
// the behavior of supported architectures, so their direct use is not
// recommended except when rigorously deemed absolutely necessary for
// performance reasons.

// This file contains Starboard implementations for synchronization primitives
// used in ICU.

#ifndef ICU_SOURCE_STARBOARD_MUTEX_SB_H_
#define ICU_SOURCE_STARBOARD_MUTEX_SB_H_

#include "starboard/thread_types.h"

typedef SbMutex UMutex;
typedef SbConditionVariable UConditionVar;
#define U_MUTEX_INITIALIZER SB_MUTEX_INITIALIZER
#define U_CONDITION_INITIALIZER SB_CONDITION_VARIABLE_INITIALIZER

#endif  // __ICU_SOURCE_STARBOARD_MUTEX_SB_H_
