// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// Should be included by headers that implement Posix file methods.

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_IMPL_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_IMPL_H_

#include "starboard/common/time.h"
#include "starboard/configuration.h"
#include "starboard/file.h"

#include "starboard/shared/internal_only.h"

// Ensure SbFile is typedef'd to a SbFilePrivate* that has a descriptor field.
SB_COMPILE_ASSERT(sizeof(((SbFile)0)->descriptor),
                  SbFilePrivate_must_have_descriptor);

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

inline int64_t TimeTToWindowsUsec(time_t time) {
  int64_t posix_usec = static_cast<int64_t>(time) * 1000000;
  return PosixTimeToWindowsTime(posix_usec);
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_IMPL_H_
