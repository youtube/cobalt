// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION < 16

#include "starboard/memory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(SbMemoryDeallocateAlignedTest, DeallocatesAligned) {
  const size_t kMaxAlign = 4096 + 1;
  for (size_t align = 2; align < kMaxAlign; align <<= 1) {
    void* memory = SbMemoryAllocateAligned(align, kSize);
    SbMemoryDeallocateAligned(memory);
  }
}

TEST(SbMemoryDeallocateAlignedTest, FreesNull) {
  SbMemoryDeallocateAligned(NULL);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 16
