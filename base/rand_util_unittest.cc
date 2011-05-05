// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

TEST(RandUtilTest, RandBytes) {
  const size_t buffer_size = 145;
  char buffer[buffer_size];
  memset(buffer, 0, buffer_size);
  base::RandBytes(buffer, buffer_size);
  char accumulator = 0;
  for(size_t i = 0; i < buffer_size; ++i)
    accumulator |= buffer[i];
  // In theory this test can fail, but it won't before the universe dies of
  // heat death.
  EXPECT_NE(0, accumulator);
}

TEST(RandUtilTest, RandBytesAsString) {
  std::string random_string = base::RandBytesAsString(0);
  EXPECT_EQ(0U, random_string.size());
  random_string = base::RandBytesAsString(145);
  EXPECT_EQ(145U, random_string.size());
  char accumulator = 0;
  for (size_t i = 0; i < random_string.size(); ++i)
    accumulator |= random_string[i];
  // In theory this test can fail, but it won't before the universe dies of
  // heat death.
  EXPECT_NE(0, accumulator);
}

// Make sure that it is still appropriate to use RandGenerator in conjunction
// with std::random_shuffle().
TEST(RandUtilTest, RandGeneratorForRandomShuffle) {
  EXPECT_EQ(base::RandGenerator(1), 0U);
  EXPECT_LE(std::numeric_limits<ptrdiff_t>::max(),
            std::numeric_limits<int64>::max());
}
