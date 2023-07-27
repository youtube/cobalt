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

// Not breaking these functions up because however one is implemented, the
// others should be implemented similarly.

#include "starboard/common/byte_swap.h"

#include <byteswap.h>

int16_t SbByteSwapS16(int16_t value) { return bswap_16(value); }

uint16_t SbByteSwapU16(uint16_t value) { return bswap_16(value); }

int32_t SbByteSwapS32(int32_t value) { return bswap_32(value); }

uint32_t SbByteSwapU32(uint32_t value) { return bswap_32(value); }

int64_t SbByteSwapS64(int64_t value) { return bswap_64(value); }

uint64_t SbByteSwapU64(uint64_t value) { return bswap_64(value); }
