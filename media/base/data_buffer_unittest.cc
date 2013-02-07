// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DataBufferTest, Constructors) {
  const uint8 kTestData[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
  const int kTestDataSize = arraysize(kTestData);

  scoped_refptr<DataBuffer> buffer(new DataBuffer(0));
  EXPECT_FALSE(buffer->GetData());

  scoped_refptr<DataBuffer> buffer2(new DataBuffer(kTestDataSize));
  EXPECT_EQ(0, buffer2->GetDataSize());
  EXPECT_EQ(kTestDataSize, buffer2->GetBufferSize());

  scoped_refptr<DataBuffer> buffer3(new DataBuffer(kTestData, kTestDataSize));
  EXPECT_EQ(kTestDataSize, buffer3->GetDataSize());
  EXPECT_EQ(kTestDataSize, buffer3->GetBufferSize());
  ASSERT_EQ(0, memcmp(buffer3->GetData(), kTestData, kTestDataSize));
  // Ensure we are copying the data, not just pointing to the original data.
  buffer3->GetWritableData()[0] = 0xFF;
  ASSERT_NE(0, memcmp(buffer3->GetData(), kTestData, kTestDataSize));
}

TEST(DataBufferTest, ReadingWriting) {
  const char kData[] = "hello";
  const int kDataSize = arraysize(kData);
  const char kNewData[] = "chromium";
  const int kNewDataSize = arraysize(kNewData);

  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer(new DataBuffer(kDataSize));
  ASSERT_TRUE(buffer);

  uint8* data = buffer->GetWritableData();
  ASSERT_TRUE(data);
  ASSERT_EQ(kDataSize, buffer->GetBufferSize());
  memcpy(data, kData, kDataSize);
  buffer->SetDataSize(kDataSize);
  const uint8* read_only_data = buffer->GetData();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->IsEndOfStream());

  scoped_refptr<DataBuffer> buffer2(new DataBuffer(kNewDataSize + 10));
  data = buffer2->GetWritableData();
  ASSERT_TRUE(data);
  ASSERT_EQ(kNewDataSize + 10, buffer2->GetBufferSize());
  memcpy(data, kNewData, kNewDataSize);
  buffer2->SetDataSize(kNewDataSize);
  read_only_data = buffer2->GetData();
  EXPECT_EQ(kNewDataSize, buffer2->GetDataSize());
  ASSERT_EQ(data, read_only_data);
  EXPECT_EQ(0, memcmp(read_only_data, kNewData, kNewDataSize));
}

}  // namespace media
