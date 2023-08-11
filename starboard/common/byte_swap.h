// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Byte Swap macros
//
// Specifies macros for swapping byte order. These functions are used to
// deal with endianness when performing I/O.

#ifndef STARBOARD_COMMON_BYTE_SWAP_H_
#define STARBOARD_COMMON_BYTE_SWAP_H_

#if SB_API_VERSION < 16

#include "starboard/byte_swap.h"

#else  // SB_API_VERSION < 16

#include "starboard/configuration.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: b/295440998 - Consider using compiler builtins.

static __inline uint16_t __sb_bswap_16(uint16_t __x) {
  return ((uint16_t)(__x << 8)) | __x >> 8;
}

static __inline uint32_t __sb_bswap_32(uint32_t __x) {
  return __x >> 24 | (__x >> 8 & 0xff00) | ((uint32_t)(__x << 8 & 0xff0000)) |
         ((uint32_t)(__x << 24));
}

static __inline uint64_t __sb_bswap_64(uint64_t __x) {
  return (__sb_bswap_32((uint32_t)__x) + 0ULL) << 32 | __sb_bswap_32(__x >> 32);
}

#if SB_IS(BIG_ENDIAN)
#define SB_HOST_TO_NET_S16(x) (x)
#define SB_HOST_TO_NET_U16(x) (x)
#define SB_HOST_TO_NET_S32(x) (x)
#define SB_HOST_TO_NET_U32(x) (x)
#define SB_HOST_TO_NET_S64(x) (x)
#define SB_HOST_TO_NET_U64(x) (x)
#else
#define SB_HOST_TO_NET_S16(x) __sb_bswap_16(x)
#define SB_HOST_TO_NET_U16(x) __sb_bswap_16(x)
#define SB_HOST_TO_NET_S32(x) __sb_bswap_32(x)
#define SB_HOST_TO_NET_U32(x) __sb_bswap_32(x)
#define SB_HOST_TO_NET_S64(x) __sb_bswap_64(x)
#define SB_HOST_TO_NET_U64(x) __sb_bswap_64(x)
#endif

#define SB_NET_TO_HOST_S16(x) SB_HOST_TO_NET_S16(x)
#define SB_NET_TO_HOST_U16(x) SB_HOST_TO_NET_U16(x)
#define SB_NET_TO_HOST_S32(x) SB_HOST_TO_NET_S32(x)
#define SB_NET_TO_HOST_U32(x) SB_HOST_TO_NET_U32(x)
#define SB_NET_TO_HOST_S64(x) SB_HOST_TO_NET_S64(x)
#define SB_NET_TO_HOST_U64(x) SB_HOST_TO_NET_U64(x)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION < 16

#endif  // STARBOARD_COMMON_BYTE_SWAP_H_
