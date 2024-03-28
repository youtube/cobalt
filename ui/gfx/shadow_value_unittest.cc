// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {

TEST(ShadowValueTest, GetMargin) {
  constexpr struct TestCase {
    Insets expected_margin;
    size_t shadow_count;
    ShadowValue shadows[2];
  } kTestCases[] = {
      {
          Insets(), 0, {},
      },
      {
          Insets(-2, -2, -2, -2),
          1,
          {
              {gfx::Vector2d(0, 0), 4, 0},
          },
      },
      {
          Insets(0, -1, -4, -3),
          1,
          {
              {gfx::Vector2d(1, 2), 4, 0},
          },
      },
      {
          Insets(-4, -3, 0, -1),
          1,
          {
              {gfx::Vector2d(-1, -2), 4, 0},
          },
      },
      {
          Insets(0, -1, -5, -4),
          2,
          {
              {gfx::Vector2d(1, 2), 4, 0}, {gfx::Vector2d(2, 3), 4, 0},
          },
      },
      {
          Insets(-4, -3, -5, -4),
          2,
          {
              {gfx::Vector2d(-1, -2), 4, 0}, {gfx::Vector2d(2, 3), 4, 0},
          },
      },
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    Insets margin = ShadowValue::GetMargin(
        ShadowValues(kTestCases[i].shadows,
                     kTestCases[i].shadows + kTestCases[i].shadow_count));

    EXPECT_EQ(kTestCases[i].expected_margin, margin) << " i=" << i;
  }
}

}  // namespace gfx
