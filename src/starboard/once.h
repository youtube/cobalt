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

// Onces represent initializations that should only ever happen once per
// process, in a thread safe way.

#ifndef STARBOARD_ONCE_H_
#define STARBOARD_ONCE_H_

#include "starboard/export.h"
#include "starboard/thread_types.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function pointer type for methods that can be called va the SbOnce() system.
typedef void (*SbOnceInitRoutine)(void);

// If SbOnce() was never called before on with |once_control|, this function
// will run |init_routine| in a thread-safe way and then returns true.  If
// SbOnce() was called before, the function returns (true) immediately.
// If |once_control| or |init_routine| are invalid, the function returns false.
SB_EXPORT bool SbOnce(SbOnceControl* once_control,
                      SbOnceInitRoutine init_routine);

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_ONCE_H_
