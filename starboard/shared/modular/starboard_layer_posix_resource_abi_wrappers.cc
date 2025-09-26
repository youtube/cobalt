// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_resource_abi_wrappers.h"

#include <errno.h>
#include <algorithm>

int musl_which_to_platform_which(int musl_which) {
  switch (musl_which) {
    case MUSL_PRIO_PROCESS:
      return PRIO_PROCESS;
    case MUSL_PRIO_PGRP:
      return PRIO_PGRP;
    case MUSL_PRIO_USER:
      return PRIO_USER;
    default:
      return musl_which;
  }
}

int __abi_wrap_getpriority(int which, musl_id_t who) {
  errno = 0;
  int result =
      getpriority(musl_which_to_platform_which(which), static_cast<id_t>(who));
  if (errno != 0) {
    return result;
  }

  // If getpriority returns a nice value that is outside the Linux standard
  // range of [-20,19], we clamp the return value to the nearest boundary of the
  // valid range.
  return std::clamp(result, LINUX_HIGHEST_PRIORITY, LINUX_LOWEST_PRIORITY);
}

int __abi_wrap_setpriority(int which, musl_id_t who, int prio) {
  // If a |prio| value is outside the Linux Standard range [-20,19], we will
  // clamp |prio| to the nearest boundary of the valid range.

  prio = std::clamp(prio, LINUX_HIGHEST_PRIORITY, LINUX_LOWEST_PRIORITY);

  return setpriority(musl_which_to_platform_which(which),
                     static_cast<id_t>(who), prio);
}
