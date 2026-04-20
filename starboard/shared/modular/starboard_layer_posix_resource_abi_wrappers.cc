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

#include "starboard/common/log.h"

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

int musl_resource_to_platform_resource(int musl_resource) {
  switch (musl_resource) {
    // Standard POSIX Resources
    case MUSL_RLIMIT_CPU:
      return RLIMIT_CPU;
    case MUSL_RLIMIT_FSIZE:
      return RLIMIT_FSIZE;
    case MUSL_RLIMIT_DATA:
      return RLIMIT_DATA;
    case MUSL_RLIMIT_STACK:
      return RLIMIT_STACK;
    case MUSL_RLIMIT_CORE:
      return RLIMIT_CORE;
    case MUSL_RLIMIT_NOFILE:
      return RLIMIT_NOFILE;
    case MUSL_RLIMIT_AS:
      return RLIMIT_AS;

      // Non-standard or optional resources
#if defined(RLIMIT_RSS)
    case MUSL_RLIMIT_RSS:
      return RLIMIT_RSS;
#endif
#if defined(RLIMIT_NPROC)
    case MUSL_RLIMIT_NPROC:
      return RLIMIT_NPROC;
#endif
#if defined(RLIMIT_MEMLOCK)
    case MUSL_RLIMIT_MEMLOCK:
      return RLIMIT_MEMLOCK;
#endif
#if defined(RLIMIT_LOCKS)
    case MUSL_RLIMIT_LOCKS:
      return RLIMIT_LOCKS;
#endif
#if defined(RLIMIT_SIGPENDING)
    case MUSL_RLIMIT_SIGPENDING:
      return RLIMIT_SIGPENDING;
#endif
#if defined(RLIMIT_MSGQUEUE)
    case MUSL_RLIMIT_MSGQUEUE:
      return RLIMIT_MSGQUEUE;
#endif
#if defined(RLIMIT_NICE)
    case MUSL_RLIMIT_NICE:
      return RLIMIT_NICE;
#endif
#if defined(RLIMIT_RTPRIO)
    case MUSL_RLIMIT_RTPRIO:
      return RLIMIT_RTPRIO;
#endif
#if defined(RLIMIT_RTTIME)
    case MUSL_RLIMIT_RTTIME:
      return RLIMIT_RTTIME;
#endif
    default:
      // Return an invalid value to indicate an unknown resource
      SB_LOG(WARNING) << "Unable to convert specified resource to "
                         "platform-implemented resource.";
      errno = EINVAL;
      return -1;
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

int __abi_wrap_getrlimit(int resource, struct musl_rlimit* musl_rlp) {
  int platform_resource = musl_resource_to_platform_resource(resource);
  if (platform_resource == -1) {
    return -1;
  }

  // There is no clear documentation on what happens when |musl_rlp| is nullptr
  // (some platforms return -1, some 0). To maximize portability, our wrapper
  // function requires that |musl_rlp| points to a valid rlimit object.
  if (!musl_rlp) {
    errno = EFAULT;
    return -1;
  }

  rlimit platform_rlimit;
  int ret = getrlimit(platform_resource, &platform_rlimit);
  if (ret == -1) {
    return -1;
  }

  // Since MUSL makes |MUSL_RLIM_SAVED_CUR|, |MUSL_RLIM_SAVED_MAX|, and
  // |MUSL_RLIM_INFINITY| equal to one another, we can just set these values to
  // |RLIM_INFINITY| if either of them are larger than the platform's saved
  // values.
  musl_rlp->musl_rlim_cur =
      platform_rlimit.rlim_cur >= RLIM_SAVED_CUR
          ? MUSL_RLIM_INFINITY
          : static_cast<musl_rlim_t>(platform_rlimit.rlim_cur);
  musl_rlp->musl_rlim_max =
      platform_rlimit.rlim_max >= RLIM_SAVED_MAX
          ? MUSL_RLIM_INFINITY
          : static_cast<musl_rlim_t>(platform_rlimit.rlim_max);

  return ret;
}

int __abi_wrap_setpriority(int which, musl_id_t who, int prio) {
  // If a |prio| value is outside the Linux Standard range [-20,19], we will
  // clamp |prio| to the nearest boundary of the valid range.

  prio = std::clamp(prio, LINUX_HIGHEST_PRIORITY, LINUX_LOWEST_PRIORITY);

  return setpriority(musl_which_to_platform_which(which),
                     static_cast<id_t>(who), prio);
}
