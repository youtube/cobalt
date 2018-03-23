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

#ifndef STARBOARD_SHARED_POSIX_HANDLE_EINTR_H_
#define STARBOARD_SHARED_POSIX_HANDLE_EINTR_H_

#include <errno.h>

#include "starboard/shared/internal_only.h"

#define HANDLE_EINTR(x)                                 \
  ({                                                    \
    decltype(x) __eintr_result__;                       \
    do {                                                \
      __eintr_result__ = (x);                           \
    } while (__eintr_result__ == -1 && errno == EINTR); \
    __eintr_result__;                                   \
  })

#endif  // STARBOARD_SHARED_POSIX_HANDLE_EINTR_H_
