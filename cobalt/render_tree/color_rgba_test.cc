// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/render_tree/color_rgba.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace render_tree {

TEST(ColorRGBA, ConstructFromInteger) {
  ColorRGBA color(0x10204080);
  EXPECT_FLOAT_EQ(0x10 / 255.0f, color.r());
  EXPECT_FLOAT_EQ(0x20 / 255.0f, color.g());
  EXPECT_FLOAT_EQ(0x40 / 255.0f, color.b());
  EXPECT_FLOAT_EQ(0x80 / 255.0f, color.a());
}

}  // namespace render_tree
}  // namespace cobalt
