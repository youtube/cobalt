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

// Module Overview: Starboard Byte Swap module C++ convenience layer
//
// Implements convenient templated wrappers around the core Starboard Byte Swap
// module. These functions are used to deal with endianness when performing I/O.

#ifndef STARBOARD_COMMON_BYTE_SWAP_H_
#define STARBOARD_COMMON_BYTE_SWAP_H_

#include <algorithm>

#include "starboard/byte_swap.h"
#include "starboard/types.h"

namespace starboard {

template <typename T>
T SbByteSwap(T value) {
  std::reverse(reinterpret_cast<uint8_t*>(&value),
               reinterpret_cast<uint8_t*>(&value + 1));
  return value;
}

template <>
inline int16_t SbByteSwap(int16_t value) {
  return SbByteSwapS16(value);
}

template <>
inline uint16_t SbByteSwap(uint16_t value) {
  return SbByteSwapU16(value);
}

template <>
inline int32_t SbByteSwap(int32_t value) {
  return SbByteSwapS32(value);
}

template <>
inline uint32_t SbByteSwap(uint32_t value) {
  return SbByteSwapU32(value);
}

template <>
inline int64_t SbByteSwap(int64_t value) {
  return SbByteSwapS64(value);
}

template <>
inline uint64_t SbByteSwap(uint64_t value) {
  return SbByteSwapU64(value);
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_BYTE_SWAP_H_
