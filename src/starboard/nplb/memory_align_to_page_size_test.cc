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

#include "starboard/configuration_constants.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

TEST(SbMemoryAlignToPageSizeTest, AlignsVariousSizes) {
  EXPECT_EQ(0, SbMemoryAlignToPageSize(0));
  EXPECT_EQ(kSbMemoryPageSize, SbMemoryAlignToPageSize(1));
  EXPECT_EQ(kSbMemoryPageSize, SbMemoryAlignToPageSize(kSbMemoryPageSize - 1));
  EXPECT_EQ(kSbMemoryPageSize, SbMemoryAlignToPageSize(kSbMemoryPageSize));
  EXPECT_EQ(100 * kSbMemoryPageSize,
            SbMemoryAlignToPageSize(100 * kSbMemoryPageSize));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
