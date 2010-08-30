// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kIntMin = std::numeric_limits<int>::min();
const int kIntMax = std::numeric_limits<int>::max();

}  // namespace

TEST(RandUtilTest, SameMinAndMax) {
  EXPECT_EQ(base::RandInt(0, 0), 0);
  EXPECT_EQ(base::RandInt(kIntMin, kIntMin), kIntMin);
  EXPECT_EQ(base::RandInt(kIntMax, kIntMax), kIntMax);
}

TEST(RandUtilTest, RandDouble) {
 // Force 64-bit precision, making sure we're not in a 80-bit FPU register.
 volatile double number = base::RandDouble();
 EXPECT_GT(1.0, number);
 EXPECT_LE(0.0, number);
}

// Make sure that it is still appropriate to use RandGenerator in conjunction
// with std::random_shuffle().
TEST(RandUtilTest, RandGeneratorForRandomShuffle) {
  EXPECT_EQ(base::RandGenerator(1), 0U);
  EXPECT_LE(std::numeric_limits<ptrdiff_t>::max(),
            std::numeric_limits<int64>::max());
}
