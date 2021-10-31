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

#include "starboard/common/string.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemSymbolizeTest, SunnyDay) {
  void* stack[100];
  EXPECT_LT(0, SbSystemGetStack(stack, SB_ARRAY_SIZE_INT(stack)));
  EXPECT_NE(static_cast<void*>(NULL), stack[0]);
  char buffer[1024] = {0};
  bool result = SbSystemSymbolize(stack[0], buffer, SB_ARRAY_SIZE_INT(buffer));
  if (result) {
    EXPECT_LT(0, strlen(buffer));
  }
}

TEST(SbSystemSymbolizeTest, RainyDay) {
  char buffer[1024] = {0};
  bool result = SbSystemSymbolize(NULL, buffer, SB_ARRAY_SIZE_INT(buffer));
  EXPECT_FALSE(result);
  EXPECT_EQ(0, strlen(buffer));

  // This stack pointer shouldn't have a symbol either.
  result = SbSystemSymbolize(buffer, buffer, SB_ARRAY_SIZE_INT(buffer));
  EXPECT_FALSE(result);
  EXPECT_EQ(0, strlen(buffer));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
