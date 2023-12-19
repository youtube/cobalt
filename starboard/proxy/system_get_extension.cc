// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include <stdarg.h>
#include <iostream>

#include "starboard/proxy/starboard_proxy.h"

const void* SbSystemGetExtension(const char* name) {
  ::starboard::proxy::system_get_extension_fn_type
      system_get_extension_test_double =
          ::starboard::proxy::GetSbProxy()->GetSystemGetExtension();
  if (system_get_extension_test_double != NULL) {
    return system_get_extension_test_double(name);
  } else {
    void* p =
        ::starboard::proxy::GetSbProxy()->LookupSymbol("SbSystemGetExtension");
    typedef const void* (*funcptr)(const char*);
    funcptr sb_system_get_extension = funcptr(p);
    return sb_system_get_extension(name);
  }
}
