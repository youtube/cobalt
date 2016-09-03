// Copyright 2016 Google Inc. All Rights Reserved.
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

// Test basic functionality of abs()

#include "testing/gtest/include/gtest/gtest.h"

#ifndef POEM_FULL_EMULATION
#define POEM_FULL_EMULATION (1)
#include "starboard/client_porting/poem/stdlib_poem.h"
#endif

namespace starboard {
namespace nplb {
namespace {

TEST(AbsTest, Positive) {
  EXPECT_EQ(3, abs(3));
}

TEST(AbsTest, Zero) {
  EXPECT_EQ(0, abs(0));
}

TEST(AbsTest, Negative) {
  EXPECT_EQ(3, abs(-3));
}

TEST(AbsTest, FloatPositive) {
  // this converts/truncates argument 3.41223 to 3, and then passes into abs
  EXPECT_EQ(3, abs(3.41223));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
