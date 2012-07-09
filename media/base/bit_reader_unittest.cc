// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(BitReaderTest, EmptyStreamTest) {
  BitReader reader(NULL, 0);
  uint8 value8 = 0xff;

  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(reader.Tell(), 0);
  ASSERT_TRUE(reader.ReadBits(0, &value8));
  ASSERT_TRUE(reader.SkipBits(0));
  ASSERT_EQ(reader.Tell(), 0);
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_FALSE(reader.ReadBits(1, &value8));
  ASSERT_FALSE(reader.SkipBits(1));
  ASSERT_EQ(value8, 0);
}

TEST(BitReaderTest, NormalOperationTest) {
  // 0101 0101 1001 1001 repeats 4 times
  uint8 buffer[] = {0x55, 0x99, 0x55, 0x99, 0x55, 0x99, 0x55, 0x99};
  BitReader reader(buffer, 6);  // Initialize with 6 bytes only
  uint8 value8;
  uint64 value64;

  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_EQ(reader.Tell(), 0);
  ASSERT_TRUE(reader.ReadBits(1, &value8));
  ASSERT_EQ(value8, 0);
  ASSERT_TRUE(reader.ReadBits(8, &value8));
  ASSERT_EQ(value8, 0xab);  // 1010 1011
  ASSERT_EQ(reader.Tell(), 9);
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_TRUE(reader.SkipBits(7));
  ASSERT_EQ(reader.Tell(), 16);
  ASSERT_TRUE(reader.ReadBits(32, &value64));
  ASSERT_EQ(value64, 0x55995599u);
  ASSERT_EQ(reader.Tell(), 48);
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_FALSE(reader.SkipBits(1));
  ASSERT_FALSE(reader.ReadBits(1, &value8));
  ASSERT_TRUE(reader.SkipBits(0));
  value8 = 0xff;
  ASSERT_TRUE(reader.ReadBits(0, &value8));
  ASSERT_EQ(value8, 0);

  reader.Initialize(buffer, 8);
  ASSERT_TRUE(reader.ReadBits(64, &value64));
  EXPECT_EQ(value64, 0x5599559955995599ull);
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_EQ(reader.Tell(), 64);
  EXPECT_FALSE(reader.ReadBits(1, &value8));
  EXPECT_FALSE(reader.SkipBits(1));
  EXPECT_TRUE(reader.ReadBits(0, &value8));
  EXPECT_TRUE(reader.SkipBits(0));
}

TEST(BitReaderTest, LongSkipTest) {
  uint8 buffer[] = {
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 1
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 2
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 3
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 4
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 5
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 6
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 7
    0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,  // 64 * 8
    0x87, 0x65
  };
  BitReader reader(buffer, sizeof(buffer));
  uint8 value8;

  EXPECT_TRUE(reader.SkipBits(64 * 8 + 8));
  EXPECT_EQ(reader.Tell(), 64 * 8 + 8);
  EXPECT_TRUE(reader.ReadBits(8, &value8));
  EXPECT_EQ(value8, 0x65);
  EXPECT_EQ(reader.Tell(), 64 * 8 + 16);
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_EQ(reader.NumBitsLeft(), 0);
}

TEST(BitReaderTest, ReadBeyondEndTest) {
  uint8 buffer[] = {0x12};
  BitReader reader(buffer, sizeof(buffer));
  uint8 value8;

  EXPECT_TRUE(reader.SkipBits(4));
  EXPECT_FALSE(reader.ReadBits(5, &value8));
  EXPECT_FALSE(reader.ReadBits(1, &value8));
  EXPECT_FALSE(reader.SkipBits(1));
  EXPECT_TRUE(reader.ReadBits(0, &value8));
  EXPECT_TRUE(reader.SkipBits(0));

  reader.Initialize(buffer, sizeof(buffer));
  EXPECT_TRUE(reader.SkipBits(4));
  EXPECT_FALSE(reader.SkipBits(5));
  EXPECT_FALSE(reader.ReadBits(1, &value8));
  EXPECT_FALSE(reader.SkipBits(1));
  EXPECT_TRUE(reader.ReadBits(0, &value8));
  EXPECT_TRUE(reader.SkipBits(0));
}

}  // namespace media
