// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

TEST(RangeTest, FromNSRange) {
  NSRange nsr = NSMakeRange(10, 3);
  gfx::Range r(nsr);
  EXPECT_EQ(nsr.location, r.start());
  EXPECT_EQ(13U, r.end());
  EXPECT_EQ(nsr.length, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
}

TEST(RangeTest, ToNSRange) {
  gfx::Range r(10, 12);
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(10U, nsr.location);
  EXPECT_EQ(2U, nsr.length);
}

TEST(RangeTest, ReversedToNSRange) {
  gfx::Range r(20, 10);
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(10U, nsr.location);
  EXPECT_EQ(10U, nsr.length);
}

TEST(RangeTest, FromNSRangeInvalid) {
  NSRange nsr = NSMakeRange(NSNotFound, 0);
  gfx::Range r(nsr);
  EXPECT_FALSE(r.IsValid());
}

TEST(RangeTest, ToNSRangeInvalid) {
  gfx::Range r(gfx::Range::InvalidRange());
  NSRange nsr = r.ToNSRange();
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), nsr.location);
  EXPECT_EQ(0U, nsr.length);
}
