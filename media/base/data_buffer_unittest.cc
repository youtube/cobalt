// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DataBufferTest, Constructors) {
  const int kTestSize = 10;

  scoped_refptr<DataBuffer> buffer(new DataBuffer(0));
  EXPECT_FALSE(buffer->GetData());

  scoped_refptr<DataBuffer> buffer2(new DataBuffer(kTestSize));
  EXPECT_EQ(0, buffer2->GetDataSize());
  EXPECT_EQ(kTestSize, buffer2->GetBufferSize());
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
