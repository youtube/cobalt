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

#if SB_API_VERSION < 16

#include "starboard/time.h"

#include <sys/time.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/time_internal.h"

SbTime SbTimeGetNow() {
  struct timeval time;
  if (gettimeofday(&time, NULL) != 0) {
    SB_NOTREACHED() << "Could not determine time of day.";
    return 0;
  }

  return FromTimeval(&time);
}

#endif  // SB_API_VERSION < 16
