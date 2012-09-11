// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class UploadFileElementReaderTest : public PlatformTest {
 protected:
  virtual void SetUp() OVERRIDE {
    const char kData[] = "123456789abcdefghi";
    bytes_.assign(kData, kData + arraysize(kData));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                    &temp_file_path_));
    ASSERT_EQ(
        static_cast<int>(bytes_.size()),
        file_util::WriteFile(temp_file_path_, &bytes_[0], bytes_.size()));

    reader_.reset(new UploadFileElementReader(
        temp_file_path_, 0, kuint64max, base::Time()));
    ASSERT_EQ(OK, reader_->InitSync());
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_FALSE(reader_->IsInMemory());
  }

  std::vector<char> bytes_;
  scoped_ptr<UploadElementReader> reader_;
  ScopedTempDir temp_dir_;
  FilePath temp_file_path_;
};

TEST_F(UploadFileElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  std::vector<char> buf(kHalfSize);
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  bytes_.resize(kHalfSize);  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(0, reader_->ReadSync(&buf[0], buf.size()));
}

TEST_F(UploadFileElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  EXPECT_EQ(static_cast<int>(bytes_.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadFileElementReaderTest, Range) {
  const uint64 kOffset = 2;
  const uint64 kLength = bytes_.size() - kOffset * 3;
  reader_.reset(new UploadFileElementReader(
      temp_file_path_, kOffset, kLength, base::Time()));
  ASSERT_EQ(OK, reader_->InitSync());
  EXPECT_EQ(kLength, reader_->GetContentLength());
  EXPECT_EQ(kLength, reader_->BytesRemaining());
  std::vector<char> buf(kLength);
  EXPECT_EQ(static_cast<int>(kLength),
            reader_->ReadSync(&buf[0], kLength));
  const std::vector<char> expected(bytes_.begin() + kOffset,
                                   bytes_.begin() + kOffset + kLength);
  EXPECT_EQ(expected, buf);
}

TEST_F(UploadFileElementReaderTest, FileChanged) {
  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(temp_file_path_, &info));

  // Expect one second before the actual modification time to simulate change.
  const base::Time expected_modification_time =
      info.last_modified - base::TimeDelta::FromSeconds(1);
  reader_.reset(new UploadFileElementReader(
      temp_file_path_, 0, kuint64max, expected_modification_time));
  EXPECT_EQ(ERR_UPLOAD_FILE_CHANGED, reader_->InitSync());
}

TEST_F(UploadFileElementReaderTest, WrongPath) {
  const FilePath wrong_path(FILE_PATH_LITERAL("wrong_path"));
  reader_.reset(new UploadFileElementReader(
      wrong_path, 0, kuint64max, base::Time()));
  ASSERT_EQ(OK, reader_->InitSync());
  EXPECT_EQ(0U, reader_->GetContentLength());
  EXPECT_EQ(0U, reader_->BytesRemaining());
}

}  // namespace net
