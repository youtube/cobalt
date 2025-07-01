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

#include <algorithm>
#include <cstring>

void CopyUtsField(const char* src, size_t src_sz, char* dest, size_t dest_sz) {
  if (src == nullptr || dest == nullptr) {
    return;
  }

  if (src_sz != UTSNAME_FIELD_SIZE || dest_sz != UTSNAME_FIELD_SIZE) {
    return;
  }

  memset(dest, 0, UTSNAME_FIELD_SIZE);
  memcpy(dest, src, UTSNAME_FIELD_SIZE);
}

SB_EXPORT int __abi_wrap_uname(struct musl_utsname* musl_uts) {
  // Set EFAULT if utsname is invalid.
  if (musl_uts == NULL) {
    errno = EFAULT;
    return -1;
  }

  struct utsname uts = {0};  // The type from platform toolchain.
  int retval = uname(&uts);
  if (retval != 0) {
    return retval;
  }

  CopyUtsField(uts.sysname, sizeof(uts.sysname), musl_uts->sysname,
               sizeof(musl_uts->sysname));
  CopyUtsField(uts.nodename, sizeof(uts.nodename), musl_uts->nodename,
               sizeof(musl_uts->nodename));
  CopyUtsField(uts.release, sizeof(uts.release), musl_uts->release,
               sizeof(musl_uts->release));
  CopyUtsField(uts.version, sizeof(uts.version), musl_uts->version,
               sizeof(musl_uts->version));
  CopyUtsField(uts.machine, sizeof(uts.machine), musl_uts->machine,
               sizeof(musl_uts->machine));
  CopyUtsField(uts.domainname, sizeof(uts.domainname), musl_uts->domainname,
               sizeof(musl_uts->domainname));

  return retval;
}
