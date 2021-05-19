// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NXSWITCH_SINGLETON_H_
#define STARBOARD_NXSWITCH_SINGLETON_H_

#include "starboard/once.h"
#include "starboard/shared/internal_only.h"

namespace starboard {

// Singleton helper class.
template <typename Type>
class Singleton {
 public:
  Singleton() {}

  // This helper class assumes that Type has a function
  // "bool Initialize();" that returns false when
  // initialization fails.

  static Type* GetOrCreateInstance() {
    static SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
    static Type* s_singleton = NULL;
    struct Local {
      static void Init() {
        Type* singleton = new Type();
        if (singleton && singleton->Type::Initialize()) {
          s_singleton = singleton;
        }
      }
    };
    SbOnce(&s_once_flag, Local::Init);
    return s_singleton;
  }

  // Prevent copying and moving.
  Singleton(Singleton const&) = delete;
  Singleton& operator=(const Singleton&) = delete;
  // Prevent deleting.
  void operator delete(void* p) = delete;

 protected:
  // Prevent destroying.
  ~Singleton();
};

}  // namespace starboard

#endif  // STARBOARD_NXSWITCH_SINGLETON_H_
