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

#include <stdlib.h>
#include <sys/mman.h>

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const size_t kSize = kSbMemoryPageSize * 8;

class MadviseTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Allocate memory for use during the tests.
    memory =
        (int*)mmap(nullptr, kSize, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
  }

  void TearDown() override { EXPECT_EQ(munmap(memory, kSize), 0); }

 protected:
  int* memory;
};

TEST_F(MadviseTest, NullPtrMemory) {
  EXPECT_EQ(-1, madvise(nullptr, kSbMemoryPageSize, MADV_NORMAL));
  EXPECT_EQ(errno, ENOMEM);
}

TEST_F(MadviseTest, SunnyDay) {
  EXPECT_EQ(0, madvise(memory, kSbMemoryPageSize, MADV_NORMAL))
      << strerror(errno);
}

TEST_F(MadviseTest, SunnyDayNotAligned) {
  int* memUnaligned = memory + 1;
  EXPECT_EQ(-1, madvise(memUnaligned, kSbMemoryPageSize, MADV_NORMAL));
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(MadviseTest, SunnyDayAligned) {
  int* memAligned = memory + kSbMemoryPageSize;
  EXPECT_EQ(0, madvise(memAligned, kSbMemoryPageSize, MADV_NORMAL))
      << strerror(errno);
}

TEST_F(MadviseTest, SunnyDayRandom) {
  EXPECT_EQ(0, madvise(memory, kSbMemoryPageSize, MADV_RANDOM))
      << strerror(errno);
}

}  // namespace
}  // namespace nplb
