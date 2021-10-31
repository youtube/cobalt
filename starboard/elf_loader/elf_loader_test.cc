// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/elf_loader_impl.h"

#include "starboard/common/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12 && (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && \
    SB_CAN(MAP_EXECUTABLE_MEMORY)
namespace starboard {
namespace elf_loader {

namespace {

// TODO: implement using real shared library fro the file system.
class ElfLoaderTest : public ::testing::Test {
 protected:
  ElfLoaderTest() {}
  ~ElfLoaderTest() {}
};

TEST_F(ElfLoaderTest, Initialize) {
  EXPECT_TRUE(true);
}

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
#endif  // SB_API_VERSION >= 12 && (SB_API_VERSION >= 12
        // || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
