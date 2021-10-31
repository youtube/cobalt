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

#include "starboard/common/scoped_ptr.h"
#include "starboard/types.h"

namespace starboard {

// This is an implementation of the MurmurHash2 by Austin Appleby.
// A couple of important notes:
// 1. This hash shall produces stable hashes for all architectures of the same
//    int-endianess.
// 2. Hashes can be chained together by using the parameter prev_hash.
uint32_t MurmurHash2_32(const void* src,
                        uint32_t size,
                        uint32_t prev_hash = 0x8d6914e8);

// Same as MurmurHash2_32 but the src data must be aligned to a 4 byte boundary.
// For best performance use this function.
uint32_t MurmurHash2_32_Aligned(const void* src,
                                uint32_t size,
                                uint32_t prev_hash = 0x8d6914e8);

}  // namespace starboard

#endif  // STARBOARD_COMMON_MURMURHASH2_H_
