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

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"  // musl_sched_param, MUSL_SCHED_*

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

int __abi_wrap_sched_setscheduler(musl_pid_t pid,
                                  int policy,
                                  const musl_sched_param* param) {
  if (!param || pid < 0) {
    errno = EINVAL;
    return -1;
  }

  int native_policy;
  switch (policy) {
    case MUSL_SCHED_FIFO:
      native_policy = SCHED_FIFO;
      break;
    case MUSL_SCHED_RR:
      native_policy = SCHED_RR;
      break;
    case MUSL_SCHED_OTHER:
      native_policy = SCHED_OTHER;
      break;
#ifdef SCHED_BATCH
    case MUSL_SCHED_BATCH:
      native_policy = SCHED_BATCH;
      break;
#endif
#ifdef SCHED_IDLE
    case MUSL_SCHED_IDLE:
      native_policy = SCHED_IDLE;
      break;
#endif
    default:
      errno = EINVAL;
      return -1;
  }

  struct sched_param native_param;
  native_param.sched_priority = param->sched_priority;

  return sched_setscheduler(static_cast<pid_t>(pid), native_policy,
                            &native_param);
}
