// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DecoderBufferTest, Constructors) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_TRUE(buffer->GetData());
  EXPECT_EQ(0, buffer->GetDataSize());
  EXPECT_FALSE(buffer->IsEndOfStream());

  const int kTestSize = 10;
  scoped_refptr<DecoderBuffer> buffer3(new DecoderBuffer(kTestSize));
  ASSERT_TRUE(buffer3);
  EXPECT_EQ(kTestSize, buffer3->GetDataSize());
}

TEST(DecoderBufferTest, CreateEOSBuffer) {
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());
  EXPECT_TRUE(buffer->IsEndOfStream());
  EXPECT_FALSE(buffer->GetData());
  EXPECT_EQ(0, buffer->GetDataSize());
}

TEST(DecoderBufferTest, CopyFrom) {
  const uint8 kData[] = "hello";
  const int kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> buffer2(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
  ASSERT_TRUE(buffer2);
  EXPECT_NE(kData, buffer2->GetData());
  EXPECT_EQ(buffer2->GetDataSize(), kDataSize);
  EXPECT_EQ(0, memcmp(buffer2->GetData(), kData, kDataSize));
  EXPECT_FALSE(buffer2->IsEndOfStream());
}

#if !defined(OS_ANDROID)
TEST(DecoderBufferTest, PaddingAlignment) {
  const uint8 kData[] = "hello";
  const int kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> buffer2(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
  ASSERT_TRUE(buffer2);

  // Padding data should always be zeroed.
  for(int i = 0; i < DecoderBuffer::kPaddingSize; i++)
    EXPECT_EQ((buffer2->GetData() + kDataSize)[i], 0);

  // If the data is padded correctly we should be able to read and write past
  // the end of the data by DecoderBuffer::kPaddingSize bytes without crashing
  // or Valgrind/ASAN throwing errors.
  const uint8 kFillChar = 0xFF;
  memset(
      buffer2->GetWritableData() + kDataSize, kFillChar,
      DecoderBuffer::kPaddingSize);
  for(int i = 0; i < DecoderBuffer::kPaddingSize; i++)
    EXPECT_EQ((buffer2->GetData() + kDataSize)[i], kFillChar);

  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
      buffer2->GetData()) & (DecoderBuffer::kAlignmentSize - 1));
}
#endif

TEST(DecoderBufferTest, ReadingWriting) {
  const char kData[] = "hello";
  const int kDataSize = arraysize(kData);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(kDataSize));
  ASSERT_TRUE(buffer);

  uint8* data = buffer->GetWritableData();
  ASSERT_TRUE(data);
  ASSERT_EQ(kDataSize, buffer->GetDataSize());
  memcpy(data, kData, kDataSize);
  const uint8* read_only_data = buffer->GetData();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->IsEndOfStream());
}

TEST(DecoderBufferTest, GetDecryptConfig) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->GetDecryptConfig());
}

}  // namespace media
