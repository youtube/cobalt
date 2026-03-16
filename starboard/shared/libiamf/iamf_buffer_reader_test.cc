// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
  EXPECT_EQ(reader.pos(), 0);
  EXPECT_EQ(reader.size(), 4);
  EXPECT_EQ(reader.BytesRemaining(), 4);
  EXPECT_EQ(reader.buf(), kTestData);
  EXPECT_EQ(reader.CurrentData(), kTestData);
}

TEST(IamfBufferReaderTest, InitialStateEmpty) {
  IamfBufferReader reader(nullptr, 0);
  EXPECT_EQ(reader.pos(), 0);
  EXPECT_EQ(reader.size(), 0);
  EXPECT_EQ(reader.BytesRemaining(), 0);
  EXPECT_EQ(reader.buf(), nullptr);
  EXPECT_EQ(reader.CurrentData(), nullptr);
}

TEST(IamfBufferReaderTest, Read1AndReadByte) {
  const uint8_t kTestData[] = {0x11, 0x22, 0x33};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  uint8_t byte1;
  ASSERT_TRUE(reader.Read1(&byte1));
  EXPECT_EQ(byte1, 0x11);
  EXPECT_EQ(reader.pos(), 1);
  EXPECT_EQ(reader.BytesRemaining(), 2);

  auto byte2_opt = reader.ReadByte();
  ASSERT_TRUE(byte2_opt.has_value());
  EXPECT_EQ(*byte2_opt, 0x22);
  EXPECT_EQ(reader.pos(), 2);

  uint8_t byte3;
  ASSERT_TRUE(reader.Read1(&byte3));
  EXPECT_EQ(byte3, 0x33);
  EXPECT_EQ(reader.pos(), 3);

  // Read past the end.
  uint8_t byte4;
  EXPECT_FALSE(reader.Read1(&byte4));
  EXPECT_FALSE(reader.ReadByte().has_value());
}

TEST(IamfBufferReaderTest, Read4) {
  const uint8_t kTestData[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  uint32_t value;
  ASSERT_TRUE(reader.Read4(&value));
  // Assumes little-endian, which is checked in the class constructor.
  EXPECT_EQ(value, 0x78563412);
  EXPECT_EQ(reader.pos(), 4);

  // Not enough bytes left.
  EXPECT_FALSE(reader.Read4(&value));
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

  uint32_t value;
  ASSERT_TRUE(reader.ReadLeb128(&value));
  EXPECT_EQ(value, 2);

  auto opt_value = reader.ReadLeb128();
  ASSERT_TRUE(opt_value.has_value());
  EXPECT_EQ(*opt_value, 127);

  ASSERT_TRUE(reader.ReadLeb128(&value));
  EXPECT_EQ(value, 129);

  ASSERT_TRUE(reader.ReadLeb128(&value));
  EXPECT_EQ(value, 624485);

  EXPECT_FALSE(reader.ReadLeb128(&value));
}

TEST(IamfBufferReaderTest, ReadLeb128FailsOnTruncatedData) {
  const uint8_t kTruncatedData[] = {0x81};  // Missing second byte.
  IamfBufferReader reader(kTruncatedData, sizeof(kTruncatedData));
  uint32_t value;
  EXPECT_FALSE(reader.ReadLeb128(&value));
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

  std::string str;
  ASSERT_TRUE(reader.ReadString(&str));
  EXPECT_EQ(str, "hello");
  EXPECT_EQ(reader.pos(), 6);

  ASSERT_TRUE(reader.ReadString(&str));
  EXPECT_EQ(str, "world");
  EXPECT_EQ(reader.pos(), 12);

  EXPECT_FALSE(reader.ReadString(&str));
}

TEST(IamfBufferReaderTest, ReadStringFailsOnMissingNullTerminator) {
  const uint8_t kTestData[] = {'h', 'e', 'l', 'l', 'o'};
  IamfBufferReader reader(kTestData, sizeof(kTestData));
  std::string str;
  EXPECT_FALSE(reader.ReadString(&str));
}

TEST(IamfBufferReaderTest, SkipBytesAndSkip) {
  const uint8_t kTestData[] = {1, 2, 3, 4, 5, 6};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  ASSERT_TRUE(reader.SkipBytes(2));
  EXPECT_EQ(reader.pos(), 2);

  ASSERT_TRUE(reader.Skip(3));
  EXPECT_EQ(reader.pos(), 5);

  // Skip to the end.
  ASSERT_TRUE(reader.SkipBytes(1));
  EXPECT_EQ(reader.pos(), 6);

  // Cannot skip past the end.
  EXPECT_FALSE(reader.Skip(1));
  EXPECT_FALSE(reader.SkipBytes(1));
}

TEST(IamfBufferReaderTest, SkipLeb128AndSkipString) {
  const uint8_t kTestData[] = {0x81, 0x01, 'h', 'i', '\0'};
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  ASSERT_TRUE(reader.SkipLeb128());
  EXPECT_EQ(reader.pos(), 2);

  ASSERT_TRUE(reader.SkipString());
  EXPECT_EQ(reader.pos(), 5);
}

TEST(IamfBufferReaderTest, MixedOperations) {
  const uint8_t kTestData[] = {
      0x12, 0x34, 0x56, 0x78,        // Read4
      't',  'e',  's',  't',  '\0',  // ReadString
      0x9F, 0x02,                    // ReadLeb128 (351)
      0xFF,                          // Read1
  };
  IamfBufferReader reader(kTestData, sizeof(kTestData));

  uint32_t val4;
  ASSERT_TRUE(reader.Read4(&val4));
  EXPECT_EQ(val4, 0x78563412);
  EXPECT_EQ(reader.pos(), 4);

  std::string str;
  ASSERT_TRUE(reader.ReadString(&str));
  EXPECT_EQ(str, "test");
  EXPECT_EQ(reader.pos(), 9);

  uint32_t val_leb;
  ASSERT_TRUE(reader.ReadLeb128(&val_leb));
  EXPECT_EQ(val_leb, 351);
  EXPECT_EQ(reader.pos(), 11);

  uint8_t val1;
  ASSERT_TRUE(reader.Read1(&val1));
  EXPECT_EQ(val1, 0xFF);
  EXPECT_EQ(reader.pos(), 12);

  EXPECT_EQ(reader.BytesRemaining(), 0);
}

}  // namespace
}  // namespace starboard
