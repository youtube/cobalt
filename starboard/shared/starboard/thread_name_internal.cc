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

#include "starboard/shared/starboard/thread_name_internal.h"
#include "starboard/once.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {

namespace {
SbOnceControl s_once_control = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_thread_name_key;

void DestroyThreadName(void* value) {
  char* thread_name = reinterpret_cast<char*>(value);
  delete[] thread_name;
}

void InitializeThreadNameKey() {
  s_thread_name_key = SbThreadCreateLocalKey(&DestroyThreadName);
}
}  // namespace

char* GetThreadName() {
  SbOnce(&s_once_control, &InitializeThreadNameKey);

  char* thread_name =
      reinterpret_cast<char*>(SbThreadGetLocalValue(s_thread_name_key));
  if (thread_name == NULL) {
    thread_name = new char[kMaxThreadName];
    thread_name[0] = '\0';
    SbThreadSetLocalValue(s_thread_name_key, thread_name);
  }
  return thread_name;
}

}  // namespace shared
}  // namespace starboard
