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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_AUXV_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_AUXV_ABI_WRAPPERS_H_

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// These values are from the musl headers.
#define MUSL_AT_NULL 0
#define MUSL_AT_IGNORE 1
#define MUSL_AT_EXECFD 2
#define MUSL_AT_PHDR 3
#define MUSL_AT_PHENT 4
#define MUSL_AT_PHNUM 5
#define MUSL_AT_PAGESZ 6
#define MUSL_AT_BASE 7
#define MUSL_AT_FLAGS 8
#define MUSL_AT_ENTRY 9
#define MUSL_AT_NOTELF 10
#define MUSL_AT_UID 11
#define MUSL_AT_EUID 12
#define MUSL_AT_GID 13
#define MUSL_AT_EGID 14
#define MUSL_AT_CLKTCK 17
#define MUSL_AT_PLATFORM 15
#define MUSL_AT_HWCAP 16
#define MUSL_AT_FPUCW 18
#define MUSL_AT_DCACHEBSIZE 19
#define MUSL_AT_ICACHEBSIZE 20
#define MUSL_AT_UCACHEBSIZE 21
#define MUSL_AT_IGNOREPPC 22
#define MUSL_AT_SECURE 23
#define MUSL_AT_BASE_PLATFORM 24
#define MUSL_AT_RANDOM 25
#define MUSL_AT_HWCAP2 26
#define MUSL_AT_EXECFN 31
#define MUSL_AT_SYSINFO 32
#define MUSL_AT_SYSINFO_EHDR 33
#define MUSL_AT_L1I_CACHESHAPE 34
#define MUSL_AT_L1D_CACHESHAPE 35
#define MUSL_AT_L2_CACHESHAPE 36
#define MUSL_AT_L3_CACHESHAPE 37
#define MUSL_AT_L1I_CACHEGEOMETRY 38
#define MUSL_AT_L1D_CACHEGEOMETRY 39
#define MUSL_AT_L2_CACHEGEOMETRY 40
#define MUSL_AT_L3_CACHEGEOMETRY 41
#define MUSL_AT_MINSIGSTKSZ 51

SB_EXPORT unsigned long __abi_wrap_getauxval(unsigned long type);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_AUXV_ABI_WRAPPERS_H_
