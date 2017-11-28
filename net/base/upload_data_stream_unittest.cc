// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

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
    const int bytes_read = stream->ReadSync(buf, kTestBufferSize);
    data_read.append(buf->data(), bytes_read);
  }
  return data_read;
}

// A mock class of UploadElementReader.
class MockUploadElementReader : public UploadElementReader {
 public:
  MockUploadElementReader(int content_length, bool is_in_memory)
      : content_length_(content_length),
        bytes_remaining_(content_length),
        is_in_memory_(is_in_memory),
        init_result_(OK),
        read_result_(OK) {}

  virtual ~MockUploadElementReader() {}

  // UploadElementReader overrides.
  MOCK_METHOD1(Init, int(const CompletionCallback& callback));
  virtual uint64 GetContentLength() const override { return content_length_; }
  virtual uint64 BytesRemaining() const override { return bytes_remaining_; }
  virtual bool IsInMemory() const override { return is_in_memory_; }
  MOCK_METHOD3(Read, int(IOBuffer* buf,
                         int buf_length,
                         const CompletionCallback& callback));

  // Sets expectation to return the specified result from Init() asynchronously.
  void SetAsyncInitExpectation(int result) {
    init_result_ = result;
    EXPECT_CALL(*this, Init(_))
        .WillOnce(DoAll(Invoke(this, &MockUploadElementReader::OnInit),
                        Return(ERR_IO_PENDING)));
  }

  // Sets expectation to return the specified result from Read().
  void SetReadExpectation(int result) {
    read_result_ = result;
    EXPECT_CALL(*this, Read(_, _, _))
        .WillOnce(Invoke(this, &MockUploadElementReader::OnRead));
  }

 private:
  void OnInit(const CompletionCallback& callback) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, init_result_));
  }

  int OnRead(IOBuffer* buf,
             int buf_length,
             const CompletionCallback& callback) {
    bytes_remaining_ = std::max(0, bytes_remaining_ - read_result_);
    if (IsInMemory()) {
      return read_result_;
    } else {
      MessageLoop::current()->PostTask(FROM_HERE,
                                       base::Bind(callback, read_result_));
      return ERR_IO_PENDING;
    }
  }

  int content_length_;
  int bytes_remaining_;
  bool is_in_memory_;

  // Result value returned from Init().
  int init_result_;

  // Result value returned from Read().
  int read_result_;
};

// A mock CompletionCallback.
class MockCompletionCallback {
 public:
  MOCK_METHOD1(Run, void(int result));

  CompletionCallback CreateCallback() {
    return base::Bind(&MockCompletionCallback::Run, base::Unretained(this));
  }
};

}  // namespace

class UploadDataStreamTest : public PlatformTest {
 public:
  virtual void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void FileChangedHelper(const FilePath& file_path,
                         const base::Time& time,
                         bool error_expected);

  base::ScopedTempDir temp_dir_;
  ScopedVector<UploadElementReader> element_readers_;
};

TEST_F(UploadDataStreamTest, EmptyUploadData) {
  UploadDataStream stream(&element_readers_, 0);
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_TRUE(stream.IsInMemory());
  EXPECT_EQ(0U, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, ConsumeAllBytes) {
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  UploadDataStream stream(&element_readers_, 0);
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_TRUE(stream.IsInMemory());
  EXPECT_EQ(kTestDataSize, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream.IsEOF()) {
    int bytes_read = stream.ReadSync(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, File) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));

  UploadDataStream stream(&element_readers_, 0);
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(kTestDataSize, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream.IsEOF()) {
    int bytes_read = stream.ReadSync(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, FileSmallerThanLength) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));
  const uint64 kFakeSize = kTestDataSize*2;

  UploadFileElementReader::ScopedOverridingContentLengthForTests
      overriding_content_length(kFakeSize);

  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));

  UploadDataStream stream(&element_readers_, 0);
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(kFakeSize, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  uint64 read_counter = 0;
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream.IsEOF()) {
    int bytes_read = stream.ReadSync(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
    read_counter += bytes_read;
    EXPECT_EQ(read_counter, stream.position());
  }
  // UpdateDataStream will pad out the file with 0 bytes so that the HTTP
  // transaction doesn't hang.  Therefore we expected the full size.
  EXPECT_EQ(kFakeSize, read_counter);
  EXPECT_EQ(read_counter, stream.position());
}

TEST_F(UploadDataStreamTest, FileAndBytes) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  const uint64 kFileRangeOffset = 1;
  const uint64 kFileRangeLength = 4;
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, kFileRangeOffset, kFileRangeLength, base::Time()));

  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));

  const uint64 kStreamSize = kTestDataSize + kFileRangeLength;
  UploadDataStream stream(&element_readers_, 0);
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(kStreamSize, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream.IsEOF()) {
    int bytes_read = stream.ReadSync(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kStreamSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, Chunk) {
  const uint64 kStreamSize = kTestDataSize*2;
  UploadDataStream stream(UploadDataStream::CHUNKED, 0);
  stream.AppendChunk(kTestData, kTestDataSize, false);
  stream.AppendChunk(kTestData, kTestDataSize, true);

  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsInMemory());
  EXPECT_EQ(0U, stream.size());  // Content-Length is 0 for chunked data.
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);
  while (!stream.IsEOF()) {
    int bytes_read = stream.ReadSync(buf, kTestBufferSize);
    ASSERT_LE(0, bytes_read);  // Not an error.
  }
  EXPECT_EQ(kStreamSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

// Init() with on-memory and not-on-memory readers.
TEST_F(UploadDataStreamTest, InitAsync) {
  // Create UploadDataStream with mock readers.
  MockUploadElementReader* reader = NULL;

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(OK));
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(OK));
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(OK);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(OK);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(OK));
  element_readers_.push_back(reader);

  UploadDataStream stream(&element_readers_, 0);

  // Run Init().
  MockCompletionCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(OK)).Times(1);
  EXPECT_EQ(stream.Init(mock_callback.CreateCallback()), ERR_IO_PENDING);
  MessageLoop::current()->RunUntilIdle();
}

// Init() of a reader fails asynchronously.
TEST_F(UploadDataStreamTest, InitAsyncFailureAsync) {
  // Create UploadDataStream with a mock reader.
  MockUploadElementReader* reader = NULL;

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(ERR_FAILED);
  element_readers_.push_back(reader);

  UploadDataStream stream(&element_readers_, 0);

  // Run Init().
  MockCompletionCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(ERR_FAILED)).Times(1);
  EXPECT_EQ(stream.Init(mock_callback.CreateCallback()), ERR_IO_PENDING);
  MessageLoop::current()->RunUntilIdle();
}

// Init() of a reader fails synchronously.
TEST_F(UploadDataStreamTest, InitAsyncFailureSync) {
  // Create UploadDataStream with mock readers.
  MockUploadElementReader* reader = NULL;

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(OK);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(ERR_FAILED));
  element_readers_.push_back(reader);

  UploadDataStream stream(&element_readers_, 0);

  // Run Init().
  MockCompletionCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(ERR_FAILED)).Times(1);
  EXPECT_EQ(stream.Init(mock_callback.CreateCallback()), ERR_IO_PENDING);
  MessageLoop::current()->RunUntilIdle();
}

// Read with a buffer whose size is same as the data.
TEST_F(UploadDataStreamTest, ReadAsyncWithExactSizeBuffer) {
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  UploadDataStream stream(&element_readers_, 0);

  MockCompletionCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(_)).Times(0);

  ASSERT_EQ(OK, stream.Init(mock_callback.CreateCallback()));
  EXPECT_TRUE(stream.IsInMemory());
  EXPECT_EQ(kTestDataSize, stream.size());
  EXPECT_EQ(0U, stream.position());
  EXPECT_FALSE(stream.IsEOF());
  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestDataSize);
  int bytes_read = stream.Read(buf, kTestDataSize,
                                mock_callback.CreateCallback());
  ASSERT_EQ(static_cast<int>(kTestDataSize), bytes_read);  // Not an error.
  EXPECT_EQ(kTestDataSize, stream.position());
  ASSERT_TRUE(stream.IsEOF());
}

// Async Read() with on-memory and not-on-memory readers.
TEST_F(UploadDataStreamTest, ReadAsync) {
  // Create UploadDataStream with mock readers.
  MockUploadElementReader* reader = NULL;

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(OK));
  reader->SetReadExpectation(kTestDataSize);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(OK);
  reader->SetReadExpectation(kTestDataSize);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, true);
  EXPECT_CALL(*reader, Init(_)).WillOnce(Return(OK));
  reader->SetReadExpectation(kTestDataSize);
  element_readers_.push_back(reader);

  reader = new MockUploadElementReader(kTestDataSize, false);
  reader->SetAsyncInitExpectation(OK);
  reader->SetReadExpectation(kTestDataSize);
  element_readers_.push_back(reader);

  UploadDataStream stream(&element_readers_, 0);

  // Run Init().
  MockCompletionCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(OK)).Times(1);
  EXPECT_EQ(ERR_IO_PENDING, stream.Init(mock_callback.CreateCallback()));
  MessageLoop::current()->RunUntilIdle();

  scoped_refptr<IOBuffer> buf = new IOBuffer(kTestBufferSize);

  // Consume the first element.
  EXPECT_CALL(mock_callback, Run(kTestDataSize)).Times(0);
  EXPECT_EQ(static_cast<int>(kTestDataSize),
            stream.Read(buf, kTestDataSize, mock_callback.CreateCallback()));
  MessageLoop::current()->RunUntilIdle();

  // Consume the second element.
  EXPECT_CALL(mock_callback, Run(kTestDataSize)).Times(1);
  EXPECT_EQ(ERR_IO_PENDING,
            stream.Read(buf, kTestDataSize, mock_callback.CreateCallback()));
  MessageLoop::current()->RunUntilIdle();

  // Consume the third and the fourth elements.
  EXPECT_CALL(mock_callback, Run(kTestDataSize*2)).Times(1);
  EXPECT_EQ(ERR_IO_PENDING,
            stream.Read(buf, kTestDataSize*2, mock_callback.CreateCallback()));
  MessageLoop::current()->RunUntilIdle();
}

void UploadDataStreamTest::FileChangedHelper(const FilePath& file_path,
                                             const base::Time& time,
                                             bool error_expected) {
  // Don't use element_readers_ here, as this function is called twice, and
  // reusing element_readers_ is wrong.
  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(new UploadFileElementReader(file_path, 1, 2, time));

  UploadDataStream stream(&element_readers, 0);
  int error_code = stream.InitSync();
  if (error_expected)
    ASSERT_EQ(ERR_UPLOAD_FILE_CHANGED, error_code);
  else
    ASSERT_EQ(OK, error_code);
}

TEST_F(UploadDataStreamTest, FileChanged) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
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
}

TEST_F(UploadDataStreamTest, MultipleInit) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  // Prepare data.
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));
  UploadDataStream stream(&element_readers_, 0);

  std::string expected_data(kTestData, kTestData + kTestDataSize);
  expected_data += expected_data;

  // Call InitSync().
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read.
  EXPECT_EQ(expected_data, ReadFromUploadDataStream(&stream));
  EXPECT_TRUE(stream.IsEOF());

  // Call InitSync() again to reset.
  ASSERT_EQ(OK, stream.InitSync());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read again.
  EXPECT_EQ(expected_data, ReadFromUploadDataStream(&stream));
  EXPECT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, MultipleInitAsync) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));
  TestCompletionCallback test_callback;

  // Prepare data.
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));
  UploadDataStream stream(&element_readers_, 0);

  std::string expected_data(kTestData, kTestData + kTestDataSize);
  expected_data += expected_data;

  // Call Init().
  ASSERT_EQ(ERR_IO_PENDING, stream.Init(test_callback.callback()));
  EXPECT_EQ(OK, test_callback.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read.
  EXPECT_EQ(expected_data, ReadFromUploadDataStream(&stream));
  EXPECT_TRUE(stream.IsEOF());

  // Call Init() again to reset.
  ASSERT_EQ(ERR_IO_PENDING, stream.Init(test_callback.callback()));
  EXPECT_EQ(OK, test_callback.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read again.
  EXPECT_EQ(expected_data, ReadFromUploadDataStream(&stream));
  EXPECT_TRUE(stream.IsEOF());
}

TEST_F(UploadDataStreamTest, InitToReset) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  // Prepare data.
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));
  UploadDataStream stream(&element_readers_, 0);

  std::vector<char> expected_data(kTestData, kTestData + kTestDataSize);
  expected_data.insert(expected_data.end(), expected_data.begin(),
                       expected_data.begin() + kTestDataSize);

  // Call Init().
  TestCompletionCallback init_callback1;
  ASSERT_EQ(ERR_IO_PENDING, stream.Init(init_callback1.callback()));
  EXPECT_EQ(OK, init_callback1.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read some.
  TestCompletionCallback read_callback1;
  std::vector<char> buf(kTestDataSize + kTestDataSize/2);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(ERR_IO_PENDING, stream.Read(wrapped_buffer, buf.size(),
                                        read_callback1.callback()));
  EXPECT_EQ(static_cast<int>(buf.size()), read_callback1.WaitForResult());
  EXPECT_EQ(buf.size(), stream.position());

  // Call Init to reset the state.
  TestCompletionCallback init_callback2;
  ASSERT_EQ(ERR_IO_PENDING, stream.Init(init_callback2.callback()));
  EXPECT_EQ(OK, init_callback2.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read.
  TestCompletionCallback read_callback2;
  std::vector<char> buf2(kTestDataSize*2);
  scoped_refptr<IOBuffer> wrapped_buffer2 = new WrappedIOBuffer(&buf2[0]);
  EXPECT_EQ(ERR_IO_PENDING, stream.Read(wrapped_buffer2, buf2.size(),
                                        read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf2.size()), read_callback2.WaitForResult());
  EXPECT_EQ(expected_data, buf2);
}

TEST_F(UploadDataStreamTest, InitDuringAsyncInit) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  // Prepare data.
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));
  UploadDataStream stream(&element_readers_, 0);

  std::vector<char> expected_data(kTestData, kTestData + kTestDataSize);
  expected_data.insert(expected_data.end(), expected_data.begin(),
                       expected_data.begin() + kTestDataSize);

  // Start Init.
  TestCompletionCallback init_callback1;
  EXPECT_EQ(ERR_IO_PENDING, stream.Init(init_callback1.callback()));

  // Call Init again to cancel the previous init.
  TestCompletionCallback init_callback2;
  EXPECT_EQ(ERR_IO_PENDING, stream.Init(init_callback2.callback()));
  EXPECT_EQ(OK, init_callback2.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read.
  TestCompletionCallback read_callback2;
  std::vector<char> buf2(kTestDataSize*2);
  scoped_refptr<IOBuffer> wrapped_buffer2 = new WrappedIOBuffer(&buf2[0]);
  EXPECT_EQ(ERR_IO_PENDING, stream.Read(wrapped_buffer2, buf2.size(),
                                        read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf2.size()), read_callback2.WaitForResult());
  EXPECT_EQ(expected_data, buf2);
  EXPECT_TRUE(stream.IsEOF());

  // Make sure callbacks are not called for cancelled operations.
  EXPECT_FALSE(init_callback1.have_result());
}

TEST_F(UploadDataStreamTest, InitDuringAsyncRead) {
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &temp_file_path));
  ASSERT_EQ(static_cast<int>(kTestDataSize),
            file_util::WriteFile(temp_file_path, kTestData, kTestDataSize));

  // Prepare data.
  element_readers_.push_back(new UploadBytesElementReader(
      kTestData, kTestDataSize));
  element_readers_.push_back(new UploadFileElementReader(
      temp_file_path, 0, kuint64max, base::Time()));
  UploadDataStream stream(&element_readers_, 0);

  std::vector<char> expected_data(kTestData, kTestData + kTestDataSize);
  expected_data.insert(expected_data.end(), expected_data.begin(),
                       expected_data.begin() + kTestDataSize);

  // Call Init().
  TestCompletionCallback init_callback1;
  ASSERT_EQ(ERR_IO_PENDING, stream.Init(init_callback1.callback()));
  EXPECT_EQ(OK, init_callback1.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Start reading.
  TestCompletionCallback read_callback1;
  std::vector<char> buf(kTestDataSize*2);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(ERR_IO_PENDING, stream.Read(wrapped_buffer, buf.size(),
                                        read_callback1.callback()));

  // Call Init to cancel the previous read.
  TestCompletionCallback init_callback2;
  EXPECT_EQ(ERR_IO_PENDING, stream.Init(init_callback2.callback()));
  EXPECT_EQ(OK, init_callback2.WaitForResult());
  EXPECT_FALSE(stream.IsEOF());
  EXPECT_EQ(kTestDataSize*2, stream.size());

  // Read.
  TestCompletionCallback read_callback2;
  std::vector<char> buf2(kTestDataSize*2);
  scoped_refptr<IOBuffer> wrapped_buffer2 = new WrappedIOBuffer(&buf2[0]);
  EXPECT_EQ(ERR_IO_PENDING, stream.Read(wrapped_buffer2, buf2.size(),
                                        read_callback2.callback()));
  EXPECT_EQ(static_cast<int>(buf2.size()), read_callback2.WaitForResult());
  EXPECT_EQ(expected_data, buf2);
  EXPECT_TRUE(stream.IsEOF());

  // Make sure callbacks are not called for cancelled operations.
  EXPECT_FALSE(read_callback1.have_result());
}

}  // namespace net
