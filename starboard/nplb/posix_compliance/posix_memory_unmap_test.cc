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

/*
  The following munmap error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - SIGSEGV on Unmapped Access: Death tests for this can be flaky and aren't
  always supported.
  - Interactions with memory locked by mlock() or mlockall().
  - Interactions with typed memory objects, which are an optional POSIX feature.
*/

#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixMunmapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_ = mmap(nullptr, kSbMemoryPageSize, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    ASSERT_NE(memory_, MAP_FAILED);
  }

  void TearDown() override {
    if (memory_ != MAP_FAILED) {
      munmap(memory_, kSbMemoryPageSize);
    }
  }

  void* memory_ = MAP_FAILED;
};

TEST_F(PosixMunmapTest, SuccessUnmapsMemory) {
  int result = munmap(memory_, kSbMemoryPageSize);
  EXPECT_EQ(result, 0) << "munmap failed: " << strerror(errno);
  // Mark the memory as unmapped so TearDown doesn't try to unmap it again.
  memory_ = MAP_FAILED;
}

TEST_F(PosixMunmapTest, FailsWithZeroLength) {
  errno = 0;
  int result = munmap(memory_, 0);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixMunmapTest, FailsWithInvalidAddress) {
  ASSERT_EQ(munmap(memory_, kSbMemoryPageSize), 0);
  memory_ = MAP_FAILED;

  // Unmapping the same address again should fail because the address
  // is no longer part of a valid mapping.
  errno = 0;
  int result = munmap(memory_, kSbMemoryPageSize);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixMunmapTest, MayFailWithUnalignedAddress) {
  void* unaligned_addr = static_cast<char*>(memory_) + 1;
  // This is a "may fail" condition. We just check that if it fails,
  // it's with EINVAL. We don't assert failure.
  errno = 0;
  int result = munmap(unaligned_addr, kSbMemoryPageSize - 1);
  if (result == -1) {
    EXPECT_EQ(errno, EINVAL);
  } else {
    memory_ = MAP_FAILED;
  }
}

}  // namespace
}  // namespace nplb
