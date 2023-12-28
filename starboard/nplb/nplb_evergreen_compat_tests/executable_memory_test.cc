// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/nplb/nplb_evergreen_compat_tests/checks.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace nplb {
namespace nplb_evergreen_compat_tests {

namespace {

const size_t kSize = 30 * 1024 * 1024;
const size_t kSmallerSize = 15 * 1024 * 1024;

class ExecutableMemoryTest : public ::testing::Test {
 protected:
  ExecutableMemoryTest() {}
  ~ExecutableMemoryTest() {}
};

TEST_F(ExecutableMemoryTest, VerifyMemoryProtection) {
  void* memory =
      SbMemoryMap(kSize, kSbMemoryMapProtectWrite, "evergreen_buffer");
  ASSERT_NE(SB_MEMORY_MAP_FAILED, memory);
  memset(memory, 0, kSize);
  ASSERT_TRUE(SbMemoryProtect(
      memory, kSmallerSize, kSbMemoryMapProtectRead | kSbMemoryMapProtectExec));
  SbMemoryUnmap(memory, kSize);
  EXPECT_TRUE(true);
}

}  // namespace
}  // namespace nplb_evergreen_compat_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_IS(EVERGREEN_COMPATIBLE)
