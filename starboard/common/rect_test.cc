// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/rect.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(RectTest, DefaultConstructor) {
  constexpr Rect rect;

  EXPECT_EQ(0, rect.x);
  EXPECT_EQ(0, rect.y);
  EXPECT_EQ(0, rect.size.width);
  EXPECT_EQ(0, rect.size.height);
}

TEST(RectTest, ConstructorWidthHeight) {
  constexpr Rect rect(10, 20, 1920, 1080);

  EXPECT_EQ(10, rect.x);
  EXPECT_EQ(20, rect.y);
  EXPECT_EQ(1920, rect.size.width);
  EXPECT_EQ(1080, rect.size.height);
}

TEST(RectTest, ConstructorSize) {
  constexpr Size size(1920, 1080);
  constexpr Rect rect(10, 20, size);

  EXPECT_EQ(10, rect.x);
  EXPECT_EQ(20, rect.y);
  EXPECT_EQ(size, rect.size);
}

TEST(RectTest, EqualityOperator) {
  constexpr Rect rect1(10, 20, 1920, 1080);
  constexpr Rect rect2(10, 20, 1920, 1080);
  constexpr Rect rect3(11, 20, 1920, 1080);
  constexpr Rect rect4(10, 21, 1920, 1080);
  constexpr Rect rect5(10, 20, 1921, 1080);

  EXPECT_TRUE(rect1 == rect2);
  EXPECT_FALSE(rect1 == rect3);
  EXPECT_FALSE(rect1 == rect4);
  EXPECT_FALSE(rect1 == rect5);
}

TEST(RectTest, InequalityOperator) {
  constexpr Rect rect1(10, 20, 1920, 1080);
  constexpr Rect rect2(10, 20, 1920, 1080);
  constexpr Rect rect3(11, 20, 1920, 1080);
  constexpr Rect rect4(10, 21, 1920, 1080);
  constexpr Rect rect5(10, 20, 1921, 1080);

  EXPECT_FALSE(rect1 != rect2);
  EXPECT_TRUE(rect1 != rect3);
  EXPECT_TRUE(rect1 != rect4);
  EXPECT_TRUE(rect1 != rect5);
}

TEST(RectTest, StreamInsertion) {
  constexpr Rect rect(10, 20, 1920, 1080);
  std::stringstream ss;

  ss << rect;

  EXPECT_EQ("{x=10, y=20, size=1920x1080}", ss.str());
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE(Rect().IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, 0, 1080).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, 1920, 0).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, -10, 1080).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, 1920, -10).IsEmpty());
  EXPECT_FALSE(Rect(10, 20, 1920, 1080).IsEmpty());
}

}  // namespace
}  // namespace starboard
