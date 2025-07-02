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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNAME_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNAME_ABI_WRAPPERS_H_

#include <sys/utsname.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUSL_UTSNAME_FIELD_SIZE 257

struct musl_utsname {
  char sysname[MUSL_UTSNAME_FIELD_SIZE];
  char nodename[MUSL_UTSNAME_FIELD_SIZE];
  char release[MUSL_UTSNAME_FIELD_SIZE];
  char version[MUSL_UTSNAME_FIELD_SIZE];
  char machine[MUSL_UTSNAME_FIELD_SIZE];
  char domainname[MUSL_UTSNAME_FIELD_SIZE];
};

SB_EXPORT int __abi_wrap_uname(struct musl_utsname* musl_uts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_UNAME_ABI_WRAPPERS_H_
