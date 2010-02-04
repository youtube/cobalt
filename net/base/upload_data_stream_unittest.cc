// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "net/base/upload_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

}  // namespace

class UploadDataStreamTest : public PlatformTest {
 public:
  UploadDataStreamTest() : upload_data_(new UploadData) { }

  scoped_refptr<UploadData> upload_data_;
};

TEST_F(UploadDataStreamTest, EmptyUploadData) {
  upload_data_->AppendBytes("", 0);
  UploadDataStream stream(upload_data_);
  EXPECT_TRUE(stream.eof());
}

TEST_F(UploadDataStreamTest, ConsumeAll) {
  upload_data_->AppendBytes(kTestData, kTestDataSize);
  UploadDataStream stream(upload_data_);
  while (!stream.eof()) {
    stream.DidConsume(stream.buf_len());
  }
}

TEST_F(UploadDataStreamTest, FileSmallerThanLength) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(kTestDataSize, file_util::WriteFile(temp_file_path,
                                                kTestData, kTestDataSize));
  const uint64 kFakeSize = kTestDataSize*2;

  std::vector<UploadData::Element> elements;
  UploadData::Element element;
  element.SetToFilePath(temp_file_path);
  element.SetContentLength(kFakeSize);
  elements.push_back(element);
  upload_data_->set_elements(elements);
  EXPECT_EQ(kFakeSize, upload_data_->GetContentLength());

  UploadDataStream stream(upload_data_);
  EXPECT_FALSE(stream.eof());
  uint64 read_counter = 0;
  while (!stream.eof()) {
    read_counter += stream.buf_len();
    stream.DidConsume(stream.buf_len());
  }
  EXPECT_LT(read_counter, stream.size());

  file_util::Delete(temp_file_path, false);
}

}  // namespace net

