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

#include "starboard/shared/modular/starboard_layer_posix_utsname_abi_wrappers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void copy_uts_field(char* dest,
                    size_t dest_sz,
                    const char* src,
                    size_t src_sz) {
  if (dest == NULL || src == NULL) {
    return;
  }

  memset(dest, 0, dest_sz);
  const auto minlen = dest_sz < src_sz ? dest_sz : src_sz;
  memcpy(dest, src, minlen);
}

SB_EXPORT int __abi_wrap_uname(struct musl_utsname* musl_uts) {
  // SEGFAULT if utsname is invalid.
  if (musl_uts == NULL) {
    errno = EFAULT;
    return -1;
  }

  struct utsname uts = {0};  // The type from platform toolchain.
  int retval = uname(&uts);
  if (retval != 0) {
    return retval;
  }

  copy_uts_field(musl_uts->sysname, sizeof(musl_uts->sysname), uts.sysname,
                 sizeof(uts.sysname));
  copy_uts_field(musl_uts->nodename, sizeof(musl_uts->nodename), uts.nodename,
                 sizeof(uts.nodename));
  copy_uts_field(musl_uts->release, sizeof(musl_uts->release), uts.release,
                 sizeof(uts.release));
  copy_uts_field(musl_uts->version, sizeof(musl_uts->version), uts.version,
                 sizeof(uts.version));
  copy_uts_field(musl_uts->machine, sizeof(musl_uts->machine), uts.machine,
                 sizeof(uts.machine));
#ifdef _GNU_SOURCE
  copy_uts_field(musl_uts->domainname, sizeof(musl_uts->domainname),
                 uts.domainname, sizeof(uts.domainname));
#else
  copy_uts_field(musl_uts->domainname, sizeof(musl_uts->domainname),
                 uts.__domainname, sizeof(uts.__domainname));
#endif

  return retval;
}
