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

#include "starboard/system.h"

#include <iostream>

#include <errno.h>
#include <string.h>

#include "starboard/common/string.h"
#include "starboard/configuration.h"

int SbSystemGetErrorString(SbSystemError error,
                           char* out_string,
                           int string_length) {
  char buffer[256];

#if defined(__GLIBC__)
  char* result = strerror_r(error, buffer, SB_ARRAY_SIZE(buffer));
#else
  char* result = buffer;
  int return_value = strerror_r(error, buffer, SB_ARRAY_SIZE(buffer));
  if (return_value != 0) {
    return 0;
  }
#endif

  if (!out_string || string_length == 0) {
    return strlen(result);
  }

  return starboard::strlcpy(out_string, result, string_length);
}
