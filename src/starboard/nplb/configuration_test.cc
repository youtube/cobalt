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

// Some kind of dumb tests that make sure that some of the utility macros in
// configuration.h work for the current toolchain.

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

SB_COMPILE_ASSERT(sizeof(int32_t) < sizeof(int64_t), int32_less_than_int64);

void TheUnreferencer(int unreferenced) {
}

struct PossiblyFunnySize {
  int32_t a;
  int8_t b;
  int32_t c;
  int8_t d;
};

TEST(SbArraySizeTest, SunnyDay) {
  const size_t kArraySize = 27;
  char test[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE(test));

  int64_t test2[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE(test2));

  PossiblyFunnySize test3[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE(test3));
}

TEST(SbArraySizeIntTest, SunnyDay) {
  const int kArraySize = 27;
  char test[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE_INT(test));

  int64_t test2[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE_INT(test2));

  PossiblyFunnySize test3[kArraySize];
  EXPECT_EQ(kArraySize, SB_ARRAY_SIZE_INT(test3));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
