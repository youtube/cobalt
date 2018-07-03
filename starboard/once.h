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

// Module Overview: Starboard Once module
//
// Onces represent initializations that should only ever happen once per
// process, in a thread-safe way.

#ifndef STARBOARD_ONCE_H_
#define STARBOARD_ONCE_H_

#include "starboard/export.h"
#include "starboard/thread_types.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function pointer type for methods that can be called via the SbOnce() system.
typedef void (*SbOnceInitRoutine)(void);

// Thread-safely runs |init_routine| only once.
// - If this |once_control| has not run a function yet, this function runs
//   |init_routine| in a thread-safe way and then returns |true|.
// - If SbOnce() was called with |once_control| before, the function returns
//   |true| immediately.
// - If |once_control| or |init_routine| is invalid, the function returns
//   |false|.
SB_EXPORT bool SbOnce(SbOnceControl* once_control,
                      SbOnceInitRoutine init_routine);

#ifdef __cplusplus
// Defines a function that will initialize the indicated type once and return
// it. This initialization is thread safe if the type being created is side
// effect free.
//
// These macros CAN ONLY BE USED IN A CC file, never in a header file.
//
// Example (in cc file):
//   SB_ONCE_INITIALIZE_FUNCTION(MyClass, GetOrCreateMyClass)
//   ..
//   MyClass* instance = GetOrCreateMyClass();
//   MyClass* instance2 = GetOrCreateMyClass();
//   DCHECK_EQ(instance, instance2);
#define SB_ONCE_INITIALIZE_FUNCTION(Type, FunctionName)     \
  Type* FunctionName() {                                    \
    static SbOnceControl s_once_flag = SB_ONCE_INITIALIZER; \
    static alignas(Type) uint8_t s_singleton[sizeof(Type)]; \
    struct Local {                                          \
      static void Init() { new (s_singleton) Type(); }      \
    };                                                      \
    SbOnce(&s_once_flag, Local::Init);                      \
    return reinterpret_cast<Type*>(s_singleton);            \
  }
#endif  // __cplusplus

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_ONCE_H_
