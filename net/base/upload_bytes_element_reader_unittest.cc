// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_bytes_element_reader.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class UploadBytesElementReaderTest : public PlatformTest {
 protected:
  virtual void SetUp() OVERRIDE {
    const char kData[] = "123abc";
    bytes_.assign(kData, kData + arraysize(kData));
    reader_.reset(new UploadBytesElementReader(&bytes_[0], bytes_.size()));
    ASSERT_EQ(OK, reader_->InitSync());
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_TRUE(reader_->IsInMemory());
  }

  std::vector<char> bytes_;
  scoped_ptr<UploadElementReader> reader_;
};

TEST_F(UploadBytesElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  std::vector<char> buf(kHalfSize);
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  bytes_.resize(kHalfSize);  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadBytesElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  EXPECT_EQ(static_cast<int>(buf.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(0, reader_->ReadSync(&buf[0], buf.size()));
}

TEST_F(UploadBytesElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  EXPECT_EQ(static_cast<int>(bytes_.size()),
            reader_->ReadSync(&buf[0], buf.size()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

}  // namespace net
