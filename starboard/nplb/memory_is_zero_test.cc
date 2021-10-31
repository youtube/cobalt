// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 128;

TEST(SbMemoryIsZeroTest, MemoryIsZero) {
  int8_t buffer[kSize] = {0};

  EXPECT_TRUE(SbMemoryIsZero(&buffer, sizeof(buffer)));
}

TEST(SbMemoryIsZeroTest, MemoryIsNonZero) {
  int8_t buffer[kSize] = {0};
  buffer[kSize - 1] = 1;

  EXPECT_FALSE(SbMemoryIsZero(&buffer, sizeof(buffer)));
  EXPECT_TRUE(SbMemoryIsZero(&buffer, sizeof(buffer) - 1));
}

TEST(SbMemoryIsZeroTest, EmptyBufferIsZero) {
  EXPECT_TRUE(SbMemoryIsZero(NULL, 0));
}

TEST(SbMemoryIsZeroTest, SingleByte) {
  int8_t zero = 0;
  int8_t non_zero = 1;

  EXPECT_TRUE(SbMemoryIsZero(&zero, 1));
  EXPECT_FALSE(SbMemoryIsZero(&non_zero, 1));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
