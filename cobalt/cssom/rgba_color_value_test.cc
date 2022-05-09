// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/rgba_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {
namespace {

TEST(RgbaColorValueTest, CreateFromHslaAchromatic) {
  EXPECT_EQ(RGBAColorValue::FromHsla(60, 0, 1, 0xff)->value(), 0xffffffff);
  EXPECT_EQ(RGBAColorValue::FromHsla(180, 0, 0.8f, 0x00)->value(), 0xcccccc00);
}

TEST(RgbaColorValueTest, CreateFromHslaBlack) {
  EXPECT_EQ(RGBAColorValue::FromHsla(60, 1, 0.0, 0xff)->value(), 0x000000ff);
  EXPECT_EQ(RGBAColorValue::FromHsla(180, 0.5, 0.0, 0xff)->value(), 0x000000ff);
  EXPECT_EQ(RGBAColorValue::FromHsla(300, 0, 0.0, 0xff)->value(), 0x000000ff);
}

TEST(RgbaColorValueTest, CreateFromHsla) {
  // Red
  EXPECT_EQ(RGBAColorValue::FromHsla(0, 1.0, 0.5, 0xff)->value(), 0xff0000ff);
  // Green
  EXPECT_EQ(RGBAColorValue::FromHsla(120, 1.0, 0.5, 0xff)->value(), 0x00ff00ff);
  // Blue
  EXPECT_EQ(RGBAColorValue::FromHsla(240, 1.0, 0.5, 0xff)->value(), 0x0000ffff);
  // Red
  EXPECT_EQ(RGBAColorValue::FromHsla(360, 1.0, 0.5, 0xff)->value(), 0xff0000ff);

  // Black
}

}  // namespace
}  // namespace cssom
}  // namespace cobalt
