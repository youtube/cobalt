// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_ELF_LOADER_DL_ITERATE_PHDR_OVERRIDE_H_
#define STARBOARD_ELF_LOADER_DL_ITERATE_PHDR_OVERRIDE_H_

#include <link.h>
#include <stddef.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define SB_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#else
#define SB_NO_SANITIZE_ADDRESS
#endif

// Early initialization hook for resolving the real glibc dl_iterate_phdr.
// Safe to call multiple times.
SB_EXPORT_PLATFORM void InitDlIteratePhdrOverride();

// Override for dl_iterate_phdr that enumerates host libraries via glibc's
// dl_iterate_phdr and appends libcobalt.so using Evergreen metadata when
// loaded.
SB_EXPORT_PLATFORM SB_NO_SANITIZE_ADDRESS int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_ELF_LOADER_DL_ITERATE_PHDR_OVERRIDE_H_
