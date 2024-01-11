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

#if SB_API_VERSION < 16

#include "starboard/time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbTimeNarrowTest, SunnyDay) {
  EXPECT_EQ(0, SbTimeNarrow(1, 1000));
  EXPECT_EQ(-1, SbTimeNarrow(-1, 1000));
  EXPECT_EQ(1, SbTimeNarrow(1000, 1000));
  EXPECT_EQ(1000, SbTimeNarrow(1000000, 1000));
  EXPECT_EQ(-1000, SbTimeNarrow(-1000000, 1000));
  EXPECT_EQ(-1, SbTimeNarrow(-1000, 1000));
  EXPECT_EQ(-2, SbTimeNarrow(-1001, 1000));
  EXPECT_EQ(-1, SbTimeNarrow(-1, 1000000));
  EXPECT_EQ(-1, SbTimeNarrow(-1, 7));
  EXPECT_EQ(-2, SbTimeNarrow(-8, 7));
  EXPECT_EQ(1, SbTimeNarrow(8, 7));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 16
