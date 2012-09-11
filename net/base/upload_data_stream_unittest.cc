// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data.h"
#include "net/base/upload_file_element_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const char kTestData[] = "0123456789";
const size_t kTestDataSize = arraysize(kTestData) - 1;
const size_t kTestBufferSize = 1 << 14;  // 16KB.

// Reads data from the upload data stream, and returns the data as string.
std::string ReadFromUploadDataStream(UploadDataStream* stream) {
  std::string data_read;
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    const int bytes_read = stream->Read(buf, kTestBufferSize);
    data_read.append(buf->data(), bytes_read);
  }
  return data_read;
}

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
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_TRUE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(0U, stream->size());
  EXPECT_EQ(0U, stream->position());
  EXPECT_TRUE(stream->IsEOF());
}

TEST_F(UploadDataStreamTest, ConsumeAllBytes) {
  upload_data_->AppendBytes(kTestData, kTestDataSize);
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_TRUE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(kTestDataSize, stream->size());
  EXPECT_EQ(0U, stream->position());
  EXPECT_FALSE(stream->IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    int bytes_read = stream->Read(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kTestDataSize, stream->position());
  ASSERT_TRUE(stream->IsEOF());
}

TEST_F(UploadDataStreamTest, File) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  std::vector<UploadElement> elements;
  UploadElement element;
  element.SetToFilePath(temp_file_path);
  elements.push_back(element);
  upload_data_->SetElements(elements);

  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_FALSE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(kTestDataSize, stream->size());
  EXPECT_EQ(0U, stream->position());
  EXPECT_FALSE(stream->IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    int bytes_read = stream->Read(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kTestDataSize, stream->position());
  ASSERT_TRUE(stream->IsEOF());
  file_util::Delete(temp_file_path, false);
}

TEST_F(UploadDataStreamTest, FileSmallerThanLength) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));
  const uint64 kFakeSize = kTestDataSize*2;

  UploadFileElementReader::ScopedOverridingContentLengthForTests
      overriding_content_length(kFakeSize);

  std::vector<UploadElement> elements;
  UploadElement element;
  element.SetToFilePath(temp_file_path);
  elements.push_back(element);
  upload_data_->SetElements(elements);

  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_FALSE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(kFakeSize, stream->size());
  EXPECT_EQ(0U, stream->position());
  EXPECT_FALSE(stream->IsEOF());
  uint64 read_counter = 0;
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    int bytes_read = stream->Read(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
    read_counter += bytes_read;
    EXPECT_EQ(read_counter, stream->position());
  }
  // UpdateDataStream will pad out the file with 0 bytes so that the HTTP
  // transaction doesn't hang.  Therefore we expected the full size.
  EXPECT_EQ(kFakeSize, read_counter);
  EXPECT_EQ(read_counter, stream->position());

  file_util::Delete(temp_file_path, false);
}

TEST_F(UploadDataStreamTest, FileAndBytes) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  const uint64 kFileRangeOffset = 1;
  const uint64 kFileRangeLength = 4;
  upload_data_->AppendFileRange(
      temp_file_path, kFileRangeOffset, kFileRangeLength, base::Time());

  upload_data_->AppendBytes(kTestData, kTestDataSize);

  const uint64 kStreamSize = kTestDataSize + kFileRangeLength;
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_FALSE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(kStreamSize, stream->size());
  EXPECT_EQ(0U, stream->position());
  EXPECT_FALSE(stream->IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    int bytes_read = stream->Read(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kStreamSize, stream->position());
  ASSERT_TRUE(stream->IsEOF());

  file_util::Delete(temp_file_path, false);
}

TEST_F(UploadDataStreamTest, Chunk) {
  upload_data_->set_is_chunked(true);
  upload_data_->AppendChunk(kTestData, kTestDataSize, false);
  upload_data_->AppendChunk(kTestData, kTestDataSize, true);

  const uint64 kStreamSize = kTestDataSize*2;
  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data_));
  ASSERT_EQ(OK, stream->Init());
  EXPECT_FALSE(stream->IsInMemory());
  ASSERT_TRUE(stream.get());
  EXPECT_EQ(0U, stream->size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0U, stream->position());
  EXPECT_FALSE(stream->IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream->IsEOF()) {
    int bytes_read = stream->Read(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kStreamSize, stream->position());
  ASSERT_TRUE(stream->IsEOF());
}

void UploadDataStreamTest::FileChangedHelper(const FilePath& file_path,
                                             const base::Time& time,
                                             bool error_expected) {
  std::vector<UploadElement> elements;
  UploadElement element;
  element.SetToFilePathRange(file_path, 1, 2, time);
  elements.push_back(element);
  // Don't use upload_data_ here, as this function is called twice, and
  // reusing upload_data_ is wrong.
  scoped_refptr<UploadData> upload_data(new UploadData);
  upload_data->SetElements(elements);

  scoped_ptr<UploadDataStream> stream(new UploadDataStream(upload_data));
  int error_code = stream->Init();
  if (error_expected)
    ASSERT_EQ(ERR_UPLOAD_FILE_CHANGED, error_code);
  else
    ASSERT_EQ(OK, error_code);
}

TEST_F(UploadDataStreamTest, FileChanged) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

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

TEST_F(UploadDataStreamTest, UploadDataReused) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  // Prepare |upload_data_| that contains a file.
  std::vector<UploadElement> elements;
  UploadElement element;
  element.SetToFilePath(temp_file_path);
  elements.push_back(element);
  upload_data_->SetElements(elements);

  // Confirm that the file is read properly.
  {
    UploadDataStream stream(upload_data_);
    ASSERT_EQ(OK, stream.Init());
    EXPECT_EQ(kTestDataSize, stream.size());
    EXPECT_EQ(kTestData, ReadFromUploadDataStream(&stream));
  }

  // Reuse |upload_data_| for another UploadDataStream, and confirm that the
  // file is read properly.
  {
    UploadDataStream stream(upload_data_);
    ASSERT_EQ(OK, stream.Init());
    EXPECT_EQ(kTestDataSize, stream.size());
    EXPECT_EQ(kTestData, ReadFromUploadDataStream(&stream));
  }

  file_util::Delete(temp_file_path, false);
}

}  // namespace net
