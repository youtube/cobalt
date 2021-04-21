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

// Module Overview: Starboard Byte Swap module
//
// Specifies functions for swapping byte order. These functions are used to
// deal with endianness when performing I/O.

#ifndef STARBOARD_BYTE_SWAP_H_
#define STARBOARD_BYTE_SWAP_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_IS(BIG_ENDIAN)
#define SB_HOST_TO_NET_S16(x) (x)
#define SB_HOST_TO_NET_U16(x) (x)
#define SB_HOST_TO_NET_S32(x) (x)
#define SB_HOST_TO_NET_U32(x) (x)
#define SB_HOST_TO_NET_S64(x) (x)
#define SB_HOST_TO_NET_U64(x) (x)
#else
#define SB_HOST_TO_NET_S16(x) SbByteSwapS16(x)
#define SB_HOST_TO_NET_U16(x) SbByteSwapU16(x)
#define SB_HOST_TO_NET_S32(x) SbByteSwapS32(x)
#define SB_HOST_TO_NET_U32(x) SbByteSwapU32(x)
#define SB_HOST_TO_NET_S64(x) SbByteSwapS64(x)
#define SB_HOST_TO_NET_U64(x) SbByteSwapU64(x)
#endif

#define SB_NET_TO_HOST_S16(x) SB_HOST_TO_NET_S16(x)
#define SB_NET_TO_HOST_U16(x) SB_HOST_TO_NET_U16(x)
#define SB_NET_TO_HOST_S32(x) SB_HOST_TO_NET_S32(x)
#define SB_NET_TO_HOST_U32(x) SB_HOST_TO_NET_U32(x)
#define SB_NET_TO_HOST_S64(x) SB_HOST_TO_NET_S64(x)
#define SB_NET_TO_HOST_U64(x) SB_HOST_TO_NET_U64(x)

// TODO: Determine if these need to be inlined for performance reasons,
// and then act on that determination somehow.

// Unconditionally swaps the byte order in signed 16-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT int16_t SbByteSwapS16(int16_t value);

// Unconditionally swaps the byte order in unsigned 16-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT uint16_t SbByteSwapU16(uint16_t value);

// Unconditionally swaps the byte order in signed 32-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT int32_t SbByteSwapS32(int32_t value);

// Unconditionally swaps the byte order in unsigned 32-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT uint32_t SbByteSwapU32(uint32_t value);

// Unconditionally swaps the byte order in signed 64-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT int64_t SbByteSwapS64(int64_t value);

// Unconditionally swaps the byte order in unsigned 64-bit |value|.
// |value|: The value for which the byte order will be swapped.
SB_EXPORT uint64_t SbByteSwapU64(uint64_t value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_BYTE_SWAP_H_
