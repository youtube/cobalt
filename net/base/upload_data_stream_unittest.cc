// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/net_errors.h"
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

  void FileChangedHelper(const FilePath& file_path,
                         const base::Time& time,
                         bool error_expected);

  scoped_refptr<UploadData> upload_data_;
};

TEST_F(UploadDataStreamTest, EmptyUploadData) {
  upload_data_->AppendBytes("", 0);
  scoped_ptr<UploadDataStream> stream(
      UploadDataStream::Create(upload_data_, NULL));
  ASSERT_TRUE(stream.get());
  EXPECT_TRUE(stream->eof());
}

TEST_F(UploadDataStreamTest, ConsumeAll) {
  upload_data_->AppendBytes(kTestData, kTestDataSize);
  scoped_ptr<UploadDataStream> stream(
      UploadDataStream::Create(upload_data_, NULL));
  ASSERT_TRUE(stream.get());
  while (!stream->eof()) {
    stream->MarkConsumedAndFillBuffer(stream->buf_len());
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
  upload_data_->SetElements(elements);
  EXPECT_EQ(kFakeSize, upload_data_->GetContentLength());

  scoped_ptr<UploadDataStream> stream(
      UploadDataStream::Create(upload_data_, NULL));
  ASSERT_TRUE(stream.get());
  EXPECT_FALSE(stream->eof());
  uint64 read_counter = 0;
  while (!stream->eof()) {
    read_counter += stream->buf_len();
    stream->MarkConsumedAndFillBuffer(stream->buf_len());
  }
  // UpdateDataStream will pad out the file with 0 bytes so that the HTTP
  // transaction doesn't hang.  Therefore we expected the full size.
  EXPECT_EQ(read_counter, stream->size());

  file_util::Delete(temp_file_path, false);
}

void UploadDataStreamTest::FileChangedHelper(const FilePath& file_path,
                                             const base::Time& time,
                                             bool error_expected) {
  std::vector<UploadData::Element> elements;
  UploadData::Element element;
  element.SetToFilePathRange(file_path, 1, 2, time);
  elements.push_back(element);
  upload_data_->SetElements(elements);

  int error_code;
  scoped_ptr<UploadDataStream> stream(
      UploadDataStream::Create(upload_data_, &error_code));
  if (error_expected)
    ASSERT_TRUE(!stream.get() && error_code == net::ERR_UPLOAD_FILE_CHANGED);
  else
    ASSERT_TRUE(stream.get() && error_code == net::OK);
}

TEST_F(UploadDataStreamTest, FileChanged) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(kTestDataSize, file_util::WriteFile(temp_file_path,
                                                kTestData, kTestDataSize));

  base::PlatformFileInfo file_info;
  ASSERT_TRUE(file_util::GetFileInfo(temp_file_path, &file_info));

  // Test file not changed.
  FileChangedHelper(temp_file_path, file_info.last_modified, false);

  // Test file changed.
  FileChangedHelper(temp_file_path,
                    file_info.last_modified - base::TimeDelta::FromSeconds(1),
                    true);

  file_util::Delete(temp_file_path, false);
}

}  // namespace net
