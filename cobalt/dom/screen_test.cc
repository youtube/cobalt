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

#include "cobalt/dom/screen.h"

#include "cobalt/cssom/viewport_size.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {

TEST(ScreenTest, Size) {
  ViewportSize view_size(ViewportSize(1280, 720, 55.f, 2.f));
  scoped_refptr<Screen> screen = new Screen(view_size);

  EXPECT_FLOAT_EQ(screen->width(), 1280.0f);
  EXPECT_FLOAT_EQ(screen->height(), 720.0f);
  EXPECT_FLOAT_EQ(screen->avail_width(), 1280.0f);
  EXPECT_FLOAT_EQ(screen->avail_height(), 720.0f);
  EXPECT_EQ(screen->color_depth(), 24);
  EXPECT_EQ(screen->pixel_depth(), 24);
  EXPECT_EQ(screen->diagonal_inches(), 55.f);
  EXPECT_EQ(screen->device_pixel_ratio(), 2.f);
}

}  // namespace dom
}  // namespace cobalt
