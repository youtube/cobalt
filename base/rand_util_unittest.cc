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

#if defined(OS_POSIX)
// For unit testing purposes only.  Do not use outside of tests.
namespace base {
extern std::string RandomBytesToGUIDString(const uint64 bytes[2]);
}  // base

TEST(RandUtilTest, GUIDGeneratesAllZeroes) {
  uint64 bytes[] = { 0, 0 };
  std::string clientid = base::RandomBytesToGUIDString(bytes);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", clientid);
}

TEST(RandUtilTest, GUIDGeneratesCorrectly) {
  uint64 bytes[] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
  std::string clientid = base::RandomBytesToGUIDString(bytes);
  EXPECT_EQ("01234567-89AB-CDEF-FEDC-BA9876543210", clientid);
}
#endif

TEST(RandUtilTest, GUIDCorrectlyFormatted) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid = base::GenerateGUID();
    EXPECT_EQ(36U, guid.length());
    std::string hexchars = "0123456789ABCDEF";
    for (uint32 i = 0; i < guid.length(); ++i) {
      char current = guid.at(i);
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        EXPECT_EQ('-', current);
      } else {
        EXPECT_TRUE(std::string::npos != hexchars.find(current));
      }
    }
  }
}

TEST(RandUtilTest, GUIDBasicUniqueness) {
  const int kIterations = 10;
  for (int it = 0; it < kIterations; ++it) {
    std::string guid1 = base::GenerateGUID();
    std::string guid2 = base::GenerateGUID();
    EXPECT_EQ(36U, guid1.length());
    EXPECT_EQ(36U, guid2.length());
    EXPECT_NE(guid1, guid2);
  }
}
