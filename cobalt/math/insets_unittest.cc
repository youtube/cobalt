// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/insets.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace math {

TEST(InsetsTest, InsetsDefault) {
  Insets insets;
  EXPECT_EQ(0, insets.left());
  EXPECT_EQ(0, insets.top());
  EXPECT_EQ(0, insets.right());
  EXPECT_EQ(0, insets.bottom());
}

TEST(InsetsTest, Insets) {
  Insets insets(1, 2, 3, 4);
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(2, insets.top());
  EXPECT_EQ(3, insets.right());
  EXPECT_EQ(4, insets.bottom());
}

TEST(InsetsTest, SetInsets) {
  Insets insets;
  insets.SetInsets(1, 2, 3, 4);
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(2, insets.top());
  EXPECT_EQ(3, insets.right());
  EXPECT_EQ(4, insets.bottom());
}

TEST(InsetsTest, Add) {
  Insets insets;
  insets.SetInsets(1, 2, 3, 4);
  insets += Insets(5, 6, 7, 8);
  EXPECT_EQ(6, insets.left());
  EXPECT_EQ(8, insets.top());
  EXPECT_EQ(10, insets.right());
  EXPECT_EQ(12, insets.bottom());
}

TEST(InsetsTest, Equality) {
  Insets insets1;
  insets1.SetInsets(1, 2, 3, 4);
  Insets insets2;
  // Test operator== and operator!=.
  EXPECT_FALSE(insets1 == insets2);
  EXPECT_TRUE(insets1 != insets2);

  insets2.SetInsets(1, 2, 3, 4);
  EXPECT_TRUE(insets1 == insets2);
  EXPECT_FALSE(insets1 != insets2);
}

TEST(InsetsTest, ToString) {
  Insets insets(1, 2, 3, 4);
  EXPECT_EQ("1,2,3,4", insets.ToString());
}

}  // namespace math
}  // namespace cobalt
