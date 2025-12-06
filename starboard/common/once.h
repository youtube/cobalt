// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_ONCE_H_
#define STARBOARD_COMMON_ONCE_H_

#include <pthread.h>

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
#define SB_ONCE_INITIALIZE_FUNCTION(Type, FunctionName)    \
  Type* FunctionName() {                                   \
    static pthread_once_t s_once_flag = PTHREAD_ONCE_INIT; \
    static Type* s_singleton = NULL;                       \
    struct Local {                                         \
      static void Init() { s_singleton = new Type(); }     \
    };                                                     \
    pthread_once(&s_once_flag, Local::Init);               \
    return s_singleton;                                    \
  }
#endif  // __cplusplus

#endif  // STARBOARD_COMMON_ONCE_H_
