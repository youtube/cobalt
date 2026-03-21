// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/libiamf/iamf_buffer_reader.h"

#include <cstdint>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(IamfBufferReaderTest, InitialState) {
  const uint8_t kTestData[] = {1, 2, 3, 4};
  IamfBufferReader reader(kTestData, sizeof(kTestData));
  EXPECT_EQ(reader.pos(), 0U);
  EXPECT_EQ(reader.size(), 4U);
  EXPECT_EQ(reader.BytesRemaining(), 4U);
  EXPECT_EQ(reader.buf(), kTestData);
  EXPECT_EQ(reader.CurrentData(), kTestData);
}

TEST(IamfBufferReaderTest, InitialStateEmpty) {
  IamfBufferReader reader(nullptr, 0);
  EXPECT_EQ(reader.pos(), 0U);
  EXPECT_EQ(reader.size(), 0U);
  EXPECT_EQ(reader.BytesRemaining(), 0U);
  EXPECT_EQ(reader.buf(), nullptr);
  EXPECT_EQ(reader.CurrentData(), nullptr);
}

TEST(IamfBufferReaderTest, Read1) {
  const uint8_t kTestData[] = {0x11, 0x22};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  auto byte1_opt = reader.Read1();
  ASSERT_TRUE(byte1_opt.has_value());
  EXPECT_EQ(*byte1_opt, 0x11U);
  EXPECT_EQ(reader.pos(), 1U);
  EXPECT_EQ(reader.BytesRemaining(), 1U);

  auto byte2_opt = reader.Read1();
  ASSERT_TRUE(byte2_opt.has_value());
  EXPECT_EQ(*byte2_opt, 0x22U);
  EXPECT_EQ(reader.pos(), 2U);

  // Read past the end.
  EXPECT_FALSE(reader.Read1().has_value());
}

TEST(IamfBufferReaderTest, Read4) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  auto value_opt = reader.Read4();
  ASSERT_TRUE(value_opt.has_value());
  // The IAMF stream uses big-endian (network byte order). The test data
  // {0x12, 0x34, 0x56, 0x78} correctly represents the value 0x12345678.
  EXPECT_EQ(*value_opt, 0x12345678U);
  EXPECT_EQ(reader.pos(), 4U);

  // Not enough bytes left.
  EXPECT_FALSE(reader.Read4().has_value());
}

TEST(IamfBufferReaderTest, ReadLeb128) {
  // Test data from https://en.wikipedia.org/wiki/LEB128
  const uint8_t kTestData[] = {
      0x02,              // 2
      0x7F,              // 127
      0x81, 0x01,        // 129
      0xE5, 0x8E, 0x26,  // 624485
  };
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  auto opt_value1 = reader.ReadLeb128();
  ASSERT_TRUE(opt_value1.has_value());
  EXPECT_EQ(*opt_value1, 2U);

  auto opt_value2 = reader.ReadLeb128();
  ASSERT_TRUE(opt_value2.has_value());
  EXPECT_EQ(*opt_value2, 127U);

  auto opt_value3 = reader.ReadLeb128();
  ASSERT_TRUE(opt_value3.has_value());
  EXPECT_EQ(*opt_value3, 129U);

  auto opt_value4 = reader.ReadLeb128();
  ASSERT_TRUE(opt_value4.has_value());
  EXPECT_EQ(*opt_value4, 624485U);

  EXPECT_FALSE(reader.ReadLeb128().has_value());
}

TEST(IamfBufferReaderTest, ReadLeb128FailsOnTruncatedData) {
  const uint8_t kTruncatedData[] = {0x81};  // Missing second byte.
  IamfBufferReader reader(kTruncatedData, sizeof(kTruncatedData));
  EXPECT_FALSE(reader.ReadLeb128().has_value());
}

TEST(IamfBufferReaderTest, ReadLeb128FailsOnInvalid5ByteEncoding) {
  const uint8_t kInvalid5Byte[] = {0x81, 0x81, 0x81, 0x81, 0x10};
  IamfBufferReader reader(kInvalid5Byte, sizeof(kInvalid5Byte));
  EXPECT_FALSE(reader.ReadLeb128().has_value());
}

TEST(IamfBufferReaderTest, ReadString) {
  const uint8_t kTestData[] = {'h', 'e', 'l', 'l', 'o', '\0',
                               'w', 'o', 'r', 'l', 'd', '\0'};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  auto str1_opt = reader.ReadString();
  ASSERT_TRUE(str1_opt.has_value());
  EXPECT_EQ(*str1_opt, "hello");
  EXPECT_EQ(reader.pos(), 6U);

  auto str2_opt = reader.ReadString();
  ASSERT_TRUE(str2_opt.has_value());
  EXPECT_EQ(*str2_opt, "world");
  EXPECT_EQ(reader.pos(), 12U);

  EXPECT_FALSE(reader.ReadString().has_value());
}

TEST(IamfBufferReaderTest, ReadStringFailsOnMissingNullTerminator) {
  const uint8_t kTestData[] = {'h', 'e', 'l', 'l', 'o'};
  IamfBufferReader reader(kTestData, sizeof(kTestData));
  EXPECT_FALSE(reader.ReadString().has_value());
}

TEST(IamfBufferReaderTest, Skip) {
  const uint8_t kTestData[] = {1, 2, 3, 4, 5, 6};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  ASSERT_TRUE(reader.Skip(2));
  EXPECT_EQ(reader.pos(), 2U);

  ASSERT_TRUE(reader.Skip(3));
  EXPECT_EQ(reader.pos(), 5U);

  // Skip to the end.
  ASSERT_TRUE(reader.Skip(1));
  EXPECT_EQ(reader.pos(), 6U);

  // Cannot skip past the end.
  EXPECT_FALSE(reader.Skip(1));
}

TEST(IamfBufferReaderTest, SkipByReading) {
  const uint8_t kTestData[] = {0x81, 0x01, 'h', 'i', '\0'};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  ASSERT_TRUE(reader.ReadLeb128().has_value());
  EXPECT_EQ(reader.pos(), 2U);

  ASSERT_TRUE(reader.ReadString().has_value());
  EXPECT_EQ(reader.pos(), 5U);
}

TEST(IamfBufferReaderTest, MixedOperations) {
  const uint8_t kTestData[] = {
      0x12, 0x34, 0x56, 0x78,        // Read32
      't',  'e',  's',  't',  '\0',  // ReadString
      0xDF, 0x02,                    // ReadLeb128 (351)
      0xFF,                          // Read8
  };
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  auto val32 = reader.Read4();
  ASSERT_TRUE(val32.has_value());
  EXPECT_EQ(*val32, 0x12345678U);
  EXPECT_EQ(reader.pos(), 4U);

  auto str = reader.ReadString();
  ASSERT_TRUE(str.has_value());
  EXPECT_EQ(*str, "test");
  EXPECT_EQ(reader.pos(), 9U);

  auto val_leb = reader.ReadLeb128();
  ASSERT_TRUE(val_leb.has_value());
  EXPECT_EQ(*val_leb, 351U);
  EXPECT_EQ(reader.pos(), 11U);

  auto val8 = reader.Read1();
  ASSERT_TRUE(val8.has_value());
  EXPECT_EQ(*val8, 0xFFU);
  EXPECT_EQ(reader.pos(), 12U);

  EXPECT_EQ(reader.BytesRemaining(), 0U);
}

}  // namespace
}  // namespace starboard
