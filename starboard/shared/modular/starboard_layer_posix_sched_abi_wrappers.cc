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

#include "starboard/shared/modular/starboard_layer_posix_sched_abi_wrappers.h"

#include <errno.h>
#include <sched.h>

#include <cstring>

int __abi_wrap_sched_getaffinity(musl_pid_t pid,
                                 size_t cpusetsize,
                                 musl_cpu_set_t* mask) {
  if (!mask) {
    errno = EFAULT;
    return -1;
  }

  cpu_set_t platform_mask;
  size_t platform_cpusetsize = sizeof(platform_mask);
  if (cpusetsize < platform_cpusetsize) {
    errno = EINVAL;
    return -1;
  }

  if ((sched_getaffinity(static_cast<pid_t>(pid), platform_cpusetsize,
                         &platform_mask)) == -1) {
    return -1;
  }

  memset(mask, 0, cpusetsize);
  memcpy(mask, &platform_mask, platform_cpusetsize);
  return 0;
}
