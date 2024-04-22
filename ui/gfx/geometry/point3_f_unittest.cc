// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point3_f.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(Point3FTest, VectorArithmetic) {
  gfx::Point3F a(1.6f, 5.1f, 3.2f);
  gfx::Vector3dF v1(3.1f, -3.2f, 9.3f);
  gfx::Vector3dF v2(-8.1f, 1.2f, 3.3f);

  static const struct {
    gfx::Point3F expected;
    gfx::Point3F actual;
  } tests[] = {
    { gfx::Point3F(4.7f, 1.9f, 12.5f), a + v1 },
    { gfx::Point3F(-1.5f, 8.3f, -6.1f), a - v1 },
    { a, a - v1 + v1 },
    { a, a + v1 - v1 },
    { a, a + gfx::Vector3dF() },
    { gfx::Point3F(12.8f, 0.7f, 9.2f), a + v1 - v2 },
    { gfx::Point3F(-9.6f, 9.5f, -2.8f), a - v1 + v2 }
  };

  for (size_t i = 0; i < std::size(tests); ++i)
    EXPECT_EQ(tests[i].expected.ToString(),
              tests[i].actual.ToString());

  a += v1;
  EXPECT_EQ(Point3F(4.7f, 1.9f, 12.5f).ToString(), a.ToString());

  a -= v2;
  EXPECT_EQ(Point3F(12.8f, 0.7f, 9.2f).ToString(), a.ToString());
}

TEST(Point3FTest, VectorFromPoints) {
  gfx::Point3F a(1.6f, 5.2f, 3.2f);
  gfx::Vector3dF v1(3.1f, -3.2f, 9.3f);

  gfx::Point3F b(a + v1);
  EXPECT_EQ((b - a).ToString(), v1.ToString());
}

TEST(Point3FTest, Scale) {
  EXPECT_EQ(Point3F().ToString(), ScalePoint(Point3F(), 2.f).ToString());
  EXPECT_EQ(Point3F().ToString(),
            ScalePoint(Point3F(), 2.f, 2.f, 2.f).ToString());

  EXPECT_EQ(Point3F(2.f, -2.f, 4.f).ToString(),
            ScalePoint(Point3F(1.f, -1.f, 2.f), 2.f).ToString());
  EXPECT_EQ(Point3F(2.f, -3.f, 8.f).ToString(),
            ScalePoint(Point3F(1.f, -1.f, 2.f), 2.f, 3.f, 4.f).ToString());

  Point3F zero;
  zero.Scale(2.f);
  zero.Scale(6.f, 3.f, 1.5f);
  EXPECT_EQ(Point3F().ToString(), zero.ToString());

  Point3F point(1.f, -1.f, 2.f);
  point.Scale(2.f);
  point.Scale(6.f, 3.f, 1.5f);
  EXPECT_EQ(Point3F(12.f, -6.f, 6.f).ToString(), point.ToString());
}

TEST(Point3FTest, IsOrigin) {
  EXPECT_TRUE(Point3F().IsOrigin());
  EXPECT_FALSE(Point3F(0, 0, 0.1f).IsOrigin());
  EXPECT_FALSE(Point3F(0, 0.1f, 0).IsOrigin());
  EXPECT_FALSE(Point3F(0.1f, 0, 0).IsOrigin());
  EXPECT_FALSE(Point3F(0, 0, -0.1f).IsOrigin());
  EXPECT_FALSE(Point3F(0, -0.1f, 0).IsOrigin());
  EXPECT_FALSE(Point3F(-0.1f, 0, 0).IsOrigin());
}

TEST(Point3FTest, OffsetFromOrigin) {
  EXPECT_EQ(Vector3dF(1.25f, 2.5f, -3.75f),
            Point3F(1.25f, 2.5f, -3.75f).OffsetFromOrigin());
}

TEST(Point3FTest, ToString) {
  EXPECT_EQ("1.03125,2.5,-3", Point3F(1.03125, 2.5, -3).ToString());
}

}  // namespace gfx
