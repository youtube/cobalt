// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_MURMURHASH2_H_
#define STARBOARD_COMMON_MURMURHASH2_H_

#include <stdint.h>
#include "starboard/common/scoped_ptr.h"

namespace starboard {

// This is an implementation of the MurmurHash2 by Austin Appleby.
// A couple of important notes:
// 1. This hash shall produces stable hashes for all architectures of the same
//    int-endianess.
// 2. Hashes can be chained together by using the parameter prev_hash.
inline uint32_t MurmurHash2_32(const void* src,
                               uint32_t size,
                               uint32_t prev_hash = 0x8d6914e8);

// Same as MurmurHash2_32 but the src data must be aligned to a 4 byte boundary.
// For best performance use this function.
inline uint32_t MurmurHash2_32_Aligned(const void* src,
                                       uint32_t size,
                                       uint32_t prev_hash = 0x8d6914e8);

//////////////////////////////// Implementation ////////////////////////////////
// Aligns data if it isn't already.
inline uint32_t MurmurHash2_32(const void* src,
                               uint32_t size,
                               uint32_t prev_hash) {
  if ((reinterpret_cast<uintptr_t>(src) & 0x3) == 0) {
    // Already aligned.
    return MurmurHash2_32_Aligned(src, size, prev_hash);
  } else {
    enum { kInlineSize = 1024 };
    if (size <= kInlineSize) {
      uint32_t aligned_src[kInlineSize / 4];
      SbMemoryCopy(aligned_src, src, size);
      return MurmurHash2_32_Aligned(aligned_src, size, prev_hash);
    } else {
      scoped_array<char> aligned_src(new char[size]);
      SbMemoryCopy(aligned_src.get(), src, size);
      return MurmurHash2_32_Aligned(aligned_src.get(), size, prev_hash);
    }
  }
}

// Warning - do not change the output of this function because certain platforms
// rely on this hash being stable across runs.
inline uint32_t MurmurHash2_32_Aligned(const void* src,
                                       uint32_t size,
                                       uint32_t prev_hash) {
  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.
  static const uint32_t m = 0x5bd1e995;
  static const int r = 24;

  const uint8_t* data = reinterpret_cast<const uint8_t*>(src);

  // Initialize the hash to the previous hash value.
  uint32_t h = prev_hash;

  // Mix 4 bytes at a time into the hash
  while (size >= 4) {
    uint32_t k = *reinterpret_cast<const uint32_t*>(data);

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    size -= 4;
    data += 4;
  }

  // Handle the last few bytes of the input array
  switch (size) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
    h *= m;
  }

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.
  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_MURMURHASH2_H_
