// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/sabi_string.h"

#include <cstring>

#include "starboard/common/log.h"
#include "starboard/string.h"

extern "C" {

const char* GetEvergreenSabiString() {
#if SB_API_VERSION >= 12
  return SB_SABI_JSON_ID;
#else
  return NULL;
#endif  // SB_API_VERSION >= 12
}

}  // extern "C"

bool CheckSabi(const char* (*sabi_function)()) {
#if SB_API_VERSION >= 12
  if (!sabi_function) {
    SB_LOG(ERROR) << "Missing sabi_function";
    return false;
  }
  SB_DLOG(INFO) << "sabi_function result: " << sabi_function();
  if (strcmp(sabi_function(), SB_SABI_JSON_ID) != 0) {
    SB_LOG(ERROR) << "Expected SB_SABI_JSON_ID=" << SB_SABI_JSON_ID;
    SB_LOG(ERROR) << "Actual   SB_SABI_JSON_ID=" << sabi_function();
    return false;
  }
  return true;
#else
  return false;
#endif  // SB_API_VERSION >= 12
}
