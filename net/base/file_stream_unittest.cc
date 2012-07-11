// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

// Creates an IOBufferWithSize that contains the kTestDataSize.
IOBufferWithSize* CreateTestDataBuffer() {
  IOBufferWithSize* buf = new IOBufferWithSize(kTestDataSize);
  memcpy(buf->data(), kTestData, kTestDataSize);
  return buf;
}

// This NetLog is used for notifying when a file stream is closed
// (i.e. TYPE_FILE_STREAM_CLOSE event is recorded).
class NetLogForNotifyingFileClosure : public NetLog {
 public:
  NetLogForNotifyingFileClosure()
      : last_id_(0),
        on_closure_(false /* manual_reset */, false /* initially_signaled */) {
  }

  // Wait until a file closure event is recorded.
  bool WaitForClosure() {
    const base::TimeDelta timeout(TestTimeouts::action_max_timeout());
    return on_closure_.TimedWait(timeout);
  }

  // NetLog overrides:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE {
    if (entry.type() == TYPE_FILE_STREAM_CLOSE)
      on_closure_.Signal();
  }

  virtual uint32 NextID() OVERRIDE { return ++last_id_; }
  virtual LogLevel GetLogLevel() const OVERRIDE { return LOG_ALL; }
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer,
                                     LogLevel log_level) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void SetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  uint32 last_id_;
  base::WaitableEvent on_closure_;
};

}  // namespace

class FileStreamTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    file_util::CreateTemporaryFile(&temp_file_path_);
    file_util::WriteFile(temp_file_path_, kTestData, kTestDataSize);
  }
  virtual void TearDown() {
    file_util::Delete(temp_file_path_, false);

    PlatformTest::TearDown();
  }

  const FilePath temp_file_path() const { return temp_file_path_; }

 private:
  FilePath temp_file_path_;
};

namespace {

TEST_F(FileStreamTest, BasicOpenClose) {
  base::PlatformFile file = base::kInvalidPlatformFileValue;
  {
    FileStream stream(NULL);
    int rv = stream.OpenSync(temp_file_path(),
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(stream.IsOpen());
    file = stream.GetPlatformFileForTesting();
  }
  EXPECT_NE(base::kInvalidPlatformFileValue, file);
  base::PlatformFileInfo info;
  // The file should be closed.
  EXPECT_FALSE(base::GetPlatformFileInfo(file, &info));
}

TEST_F(FileStreamTest, FileHandleLeftOpen) {
  bool created = false;
  ASSERT_EQ(kTestDataSize,
      file_util::WriteFile(temp_file_path(), kTestData, kTestDataSize));
  int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ;
  base::PlatformFile file = base::CreatePlatformFile(
      temp_file_path(), flags, &created, NULL);

  {
    // Seek to the beginning of the file and read.
    FileStream read_stream(file, flags, NULL);
    EXPECT_TRUE(read_stream.IsOpen());
  }

  EXPECT_NE(base::kInvalidPlatformFileValue, file);
  base::PlatformFileInfo info;
  // The file should still be open.
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &info));
  // Clean up.
  EXPECT_TRUE(base::ClosePlatformFile(file));
}

// Test the use of FileStream with a file handle provided at construction.
TEST_F(FileStreamTest, UseFileHandle) {
  bool created = false;

  // 1. Test reading with a file handle.
  ASSERT_EQ(kTestDataSize,
      file_util::WriteFile(temp_file_path(), kTestData, kTestDataSize));
  int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ;
  base::PlatformFile file = base::CreatePlatformFile(
      temp_file_path(), flags, &created, NULL);

  // Seek to the beginning of the file and read.
  FileStream read_stream(file, flags, NULL);
  ASSERT_EQ(0, read_stream.SeekSync(FROM_BEGIN, 0));
  ASSERT_EQ(kTestDataSize, read_stream.Available());
  // Read into buffer and compare.
  char buffer[kTestDataSize];
  ASSERT_EQ(kTestDataSize,
            read_stream.ReadSync(buffer, kTestDataSize));
  ASSERT_EQ(0, memcmp(kTestData, buffer, kTestDataSize));
  read_stream.CloseSync();

  // 2. Test writing with a file handle.
  file_util::Delete(temp_file_path(), false);
  flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE;
  file = base::CreatePlatformFile(temp_file_path(), flags, &created, NULL);

  FileStream write_stream(file, flags, NULL);
  ASSERT_EQ(0, write_stream.SeekSync(FROM_BEGIN, 0));
  ASSERT_EQ(kTestDataSize,
            write_stream.WriteSync(kTestData, kTestDataSize));
  write_stream.CloseSync();

  // Read into buffer and compare to make sure the handle worked fine.
  ASSERT_EQ(kTestDataSize,
      file_util::ReadFile(temp_file_path(), buffer, kTestDataSize));
  ASSERT_EQ(0, memcmp(kTestData, buffer, kTestDataSize));
}

TEST_F(FileStreamTest, UseClosedStream) {
  FileStream stream(NULL);

  EXPECT_FALSE(stream.IsOpen());

  // Try seeking...
  int64 new_offset = stream.SeekSync(FROM_BEGIN, 5);
  EXPECT_EQ(ERR_UNEXPECTED, new_offset);

  // Try available...
  int64 avail = stream.Available();
  EXPECT_EQ(ERR_UNEXPECTED, avail);

  // Try reading...
  char buf[10];
  int rv = stream.ReadSync(buf, arraysize(buf));
  EXPECT_EQ(ERR_UNEXPECTED, rv);
}

TEST_F(FileStreamTest, BasicRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.ReadSync(buf, arraysize(buf));
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(FileStreamTest, AsyncRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestCompletionCallback callback;

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
    rv = stream.Read(buf, buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(FileStreamTest, AsyncRead_EarlyClose) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestCompletionCallback callback;

  scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
  rv = stream.Read(buf, buf->size(), callback.callback());
  stream.CloseSync();
  if (rv < 0) {
    EXPECT_EQ(ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    EXPECT_EQ(std::string(kTestData, rv), std::string(buf->data(), rv));
  }
}

// Similar to AsyncRead_EarlyClose but deletes a stream instead, to ensure
// that deleting a stream is safe while an async read is in flight.
TEST_F(FileStreamTest, AsyncRead_EarlyDelete) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  scoped_ptr<FileStream> stream(new FileStream(NULL));
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback callback;
  int rv = stream->Open(temp_file_path(), flags, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  int64 total_bytes_avail = stream->Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
  rv = stream->Read(buf, buf->size(), callback.callback());
  stream.reset();  // Delete instead of closing it.
  if (rv < 0) {
    EXPECT_EQ(ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    EXPECT_EQ(std::string(kTestData, rv), std::string(buf->data(), rv));
  }
}

// Similar to AsyncRead_EarlyDelete but using a given file handler rather than
// calling FileStream::Open, to ensure that deleting a stream with in-flight
// operation without auto-closing feature is also ok.
TEST_F(FileStreamTest, AsyncRead_EarlyDelete_NoAutoClose) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  bool created = false;
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  base::PlatformFileError error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file = base::CreatePlatformFile(
      temp_file_path(), flags, &created, &error_code);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error_code);

  scoped_ptr<FileStream> stream(new FileStream(file, flags, NULL));
  int64 total_bytes_avail = stream->Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestCompletionCallback callback;
  scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
  int rv = stream->Read(buf, buf->size(), callback.callback());
  stream.reset();  // Delete instead of closing it.
  if (rv < 0) {
    EXPECT_EQ(ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    EXPECT_EQ(std::string(kTestData, rv), std::string(buf->data(), rv));
  }

  base::PlatformFileInfo info;
  // The file should still be open.
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &info));
  // Clean up.
  EXPECT_TRUE(base::ClosePlatformFile(file));
}

TEST_F(FileStreamTest, BasicRead_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  const int64 kOffset = 3;
  int64 new_offset = stream.SeekSync(FROM_BEGIN, kOffset);
  EXPECT_EQ(kOffset, new_offset);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size - kOffset, total_bytes_avail);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.ReadSync(buf, arraysize(buf));
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData + kOffset);
  EXPECT_EQ(kTestData + kOffset, data_read);
}

TEST_F(FileStreamTest, AsyncRead_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  TestInt64CompletionCallback callback64;
  const int64 kOffset = 3;
  rv = stream.Seek(FROM_BEGIN, kOffset, callback64.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  int64 new_offset = callback64.WaitForResult();
  EXPECT_EQ(kOffset, new_offset);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size - kOffset, total_bytes_avail);

  TestCompletionCallback callback;

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
    rv = stream.Read(buf, buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_EQ(kTestData + kOffset, data_read);
}

TEST_F(FileStreamTest, SeekAround) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  const int64 kOffset = 3;
  int64 new_offset = stream.SeekSync(FROM_BEGIN, kOffset);
  EXPECT_EQ(kOffset, new_offset);

  new_offset = stream.SeekSync(FROM_CURRENT, kOffset);
  EXPECT_EQ(2 * kOffset, new_offset);

  new_offset = stream.SeekSync(FROM_CURRENT, -kOffset);
  EXPECT_EQ(kOffset, new_offset);

  const int kTestDataLen = arraysize(kTestData) - 1;

  new_offset = stream.SeekSync(FROM_END, -kTestDataLen);
  EXPECT_EQ(0, new_offset);
}

TEST_F(FileStreamTest, AsyncSeekAround) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_ASYNC |
              base::PLATFORM_FILE_READ;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  TestInt64CompletionCallback callback;

  const int64 kOffset = 3;
  rv = stream.Seek(FROM_BEGIN, kOffset, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  int64 new_offset = callback.WaitForResult();
  EXPECT_EQ(kOffset, new_offset);

  rv = stream.Seek(FROM_CURRENT, kOffset, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  new_offset = callback.WaitForResult();
  EXPECT_EQ(2 * kOffset, new_offset);

  rv = stream.Seek(FROM_CURRENT, -kOffset, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  new_offset = callback.WaitForResult();
  EXPECT_EQ(kOffset, new_offset);

  const int kTestDataLen = arraysize(kTestData) - 1;

  rv = stream.Seek(FROM_END, -kTestDataLen, callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  new_offset = callback.WaitForResult();
  EXPECT_EQ(0, new_offset);
}

TEST_F(FileStreamTest, BasicWrite) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS |
              base::PLATFORM_FILE_WRITE;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, file_size);

  rv = stream.WriteSync(kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, rv);
  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize, file_size);
}

TEST_F(FileStreamTest, AsyncWrite) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, file_size);

  TestCompletionCallback callback;
  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  scoped_refptr<DrainableIOBuffer> drainable =
      new DrainableIOBuffer(buf, buf->size());
  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(drainable, drainable->BytesRemaining(),
                      callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }
  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(file_size, total_bytes_written);
}

TEST_F(FileStreamTest, AsyncWrite_EarlyClose) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, file_size);

  TestCompletionCallback callback;

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  rv = stream.Write(buf, buf->size(), callback.callback());
  stream.CloseSync();
  if (rv < 0) {
    EXPECT_EQ(ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    ok = file_util::GetFileSize(temp_file_path(), &file_size);
    EXPECT_TRUE(ok);
    EXPECT_EQ(file_size, rv);
  }
}

TEST_F(FileStreamTest, BasicWrite_FromOffset) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_WRITE;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize, file_size);

  const int64 kOffset = 0;
  int64 new_offset = stream.SeekSync(FROM_END, kOffset);
  EXPECT_EQ(kTestDataSize, new_offset);

  rv = stream.WriteSync(kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, rv);
  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, AsyncWrite_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  TestInt64CompletionCallback callback64;
  const int64 kOffset = 0;
  rv = stream.Seek(FROM_END, kOffset, callback64.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  int64 new_offset = callback64.WaitForResult();
  EXPECT_EQ(kTestDataSize, new_offset);

  TestCompletionCallback callback;
  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  scoped_refptr<DrainableIOBuffer> drainable =
      new DrainableIOBuffer(buf, buf->size());
  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(drainable, drainable->BytesRemaining(),
                      callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }
  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(file_size, kTestDataSize * 2);
}

TEST_F(FileStreamTest, BasicReadWrite) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.ReadSync(buf, arraysize(buf));
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);

  rv = stream.WriteSync(kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, rv);
  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, BasicWriteRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.SeekSync(FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  rv = stream.WriteSync(kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, rv);

  offset = stream.SeekSync(FROM_BEGIN, 0);
  EXPECT_EQ(0, offset);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.ReadSync(buf, arraysize(buf));
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);

  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

TEST_F(FileStreamTest, BasicAsyncReadWrite) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestCompletionCallback callback;
  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
    rv = stream.Read(buf, buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);

  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  scoped_refptr<DrainableIOBuffer> drainable =
      new DrainableIOBuffer(buf, buf->size());
  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(drainable, drainable->BytesRemaining(),
                      callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }

  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, BasicAsyncWriteRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestInt64CompletionCallback callback64;
  rv = stream.Seek(FROM_END, 0, callback64.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  int64 offset = callback64.WaitForResult();
  EXPECT_EQ(offset, file_size);

  TestCompletionCallback callback;
  int total_bytes_written = 0;

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  scoped_refptr<DrainableIOBuffer> drainable =
      new DrainableIOBuffer(buf, buf->size());
  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(drainable, drainable->BytesRemaining(),
                      callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    drainable->DidConsume(rv);
    total_bytes_written += rv;
  }

  EXPECT_EQ(kTestDataSize, total_bytes_written);

  rv = stream.Seek(FROM_BEGIN, 0, callback64.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  offset = callback64.WaitForResult();
  EXPECT_EQ(0, offset);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
    rv = stream.Read(buf, buf->size(), callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf->data(), rv);
  }
  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteReadCompletionCallback {
 public:
  TestWriteReadCompletionCallback(
      FileStream* stream,
      int* total_bytes_written,
      int* total_bytes_read,
      std::string* data_read)
      : result_(0),
        have_result_(false),
        waiting_for_result_(false),
        stream_(stream),
        total_bytes_written_(total_bytes_written),
        total_bytes_read_(total_bytes_read),
        data_read_(data_read),
        callback_(base::Bind(&TestWriteReadCompletionCallback::OnComplete,
                             base::Unretained(this))),
        test_data_(CreateTestDataBuffer()),
        drainable_(new DrainableIOBuffer(test_data_, kTestDataSize)) {
  }

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    DCHECK_LT(0, result);
    *total_bytes_written_ += result;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0, total_bytes_read = 0;
      std::string data_read;
      TestWriteReadCompletionCallback callback(
          stream_, &total_bytes_written, &total_bytes_read, &data_read);
      rv = stream_->Write(drainable_, drainable_->BytesRemaining(),
                          callback.callback());
      DCHECK_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      drainable_->DidConsume(total_bytes_written);
      *total_bytes_written_ += total_bytes_written;
      *total_bytes_read_ += total_bytes_read;
      *data_read_ += data_read;
    } else {  // We're done writing all data.  Start reading the data.
      stream_->SeekSync(FROM_BEGIN, 0);

      TestCompletionCallback callback;
      for (;;) {
        scoped_refptr<IOBufferWithSize> buf = new IOBufferWithSize(4);
        rv = stream_->Read(buf, buf->size(), callback.callback());
        if (rv == ERR_IO_PENDING) {
          MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
          rv = callback.WaitForResult();
        }
        EXPECT_LE(0, rv);
        if (rv <= 0)
          break;
        *total_bytes_read_ += rv;
        data_read_->append(buf->data(), rv);
      }
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

  int result_;
  bool have_result_;
  bool waiting_for_result_;
  FileStream* stream_;
  int* total_bytes_written_;
  int* total_bytes_read_;
  std::string* data_read_;
  const CompletionCallback callback_;
  scoped_refptr<IOBufferWithSize> test_data_;
  scoped_refptr<DrainableIOBuffer> drainable_;

  DISALLOW_COPY_AND_ASSIGN(TestWriteReadCompletionCallback);
};

TEST_F(FileStreamTest, AsyncWriteRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.SeekSync(FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  int total_bytes_written = 0;
  int total_bytes_read = 0;
  std::string data_read;
  TestWriteReadCompletionCallback callback(&stream, &total_bytes_written,
                                           &total_bytes_read, &data_read);

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  rv = stream.Write(buf, buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_LT(0, rv);
  EXPECT_EQ(kTestDataSize, total_bytes_written);

  stream.CloseSync();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteCloseCompletionCallback {
 public:
  TestWriteCloseCompletionCallback(FileStream* stream, int* total_bytes_written)
      : result_(0),
        have_result_(false),
        waiting_for_result_(false),
        stream_(stream),
        total_bytes_written_(total_bytes_written),
        callback_(base::Bind(&TestWriteCloseCompletionCallback::OnComplete,
                             base::Unretained(this))),
        test_data_(CreateTestDataBuffer()),
        drainable_(new DrainableIOBuffer(test_data_, kTestDataSize)) {
  }

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    DCHECK_LT(0, result);
    *total_bytes_written_ += result;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0;
      TestWriteCloseCompletionCallback callback(stream_, &total_bytes_written);
      rv = stream_->Write(drainable_, drainable_->BytesRemaining(),
                          callback.callback());
      DCHECK_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      drainable_->DidConsume(total_bytes_written);
      *total_bytes_written_ += total_bytes_written;
    } else {  // We're done writing all data.  Close the file.
      stream_->CloseSync();
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

  int result_;
  bool have_result_;
  bool waiting_for_result_;
  FileStream* stream_;
  int* total_bytes_written_;
  const CompletionCallback callback_;
  scoped_refptr<IOBufferWithSize> test_data_;
  scoped_refptr<DrainableIOBuffer> drainable_;

  DISALLOW_COPY_AND_ASSIGN(TestWriteCloseCompletionCallback);
};

TEST_F(FileStreamTest, AsyncWriteClose) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.SeekSync(FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  int total_bytes_written = 0;
  TestWriteCloseCompletionCallback callback(&stream, &total_bytes_written);

  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  rv = stream.Write(buf, buf->size(), callback.callback());
  if (rv == ERR_IO_PENDING)
    total_bytes_written = callback.WaitForResult();
  EXPECT_LT(0, total_bytes_written);
  EXPECT_EQ(kTestDataSize, total_bytes_written);

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

// Tests truncating a file.
TEST_F(FileStreamTest, Truncate) {
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;

  FileStream write_stream(NULL);
  ASSERT_EQ(OK, write_stream.OpenSync(temp_file_path(), flags));

  // Write some data to the file.
  const char test_data[] = "0123456789";
  write_stream.WriteSync(test_data, arraysize(test_data));

  // Truncate the file.
  ASSERT_EQ(4, write_stream.Truncate(4));

  // Write again.
  write_stream.WriteSync(test_data, 4);

  // Close the stream.
  write_stream.CloseSync();

  // Read in the contents and make sure we get back what we expected.
  std::string read_contents;
  EXPECT_TRUE(file_util::ReadFileToString(temp_file_path(), &read_contents));

  EXPECT_EQ("01230123", read_contents);
}

TEST_F(FileStreamTest, AsyncBasicOpenClose) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(stream.IsOpen());

  stream.Close(callback.callback());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_FALSE(stream.IsOpen());
}

TEST_F(FileStreamTest, SyncCloseTwice) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ;
  int rv = stream.OpenSync(temp_file_path(), flags);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(stream.IsOpen());

  // Closing twice should be safe.
  stream.CloseSync();
  EXPECT_FALSE(stream.IsOpen());

  stream.CloseSync();
  EXPECT_FALSE(stream.IsOpen());
}

TEST_F(FileStreamTest, AsyncCloseTwice) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(stream.IsOpen());

  // Closing twice should be safe.
  stream.Close(callback.callback());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_FALSE(stream.IsOpen());

  stream.Close(callback.callback());
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_FALSE(stream.IsOpen());
}

// TODO(satorux): This should be gone once all once all async clients are
// migrated to use Close(). crbug.com/114783
TEST_F(FileStreamTest, AsyncWriteAndCloseSync) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback callback;
  int rv = stream.Open(temp_file_path(), flags, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(stream.IsOpen());

  // Write some data asynchronously.
  scoped_refptr<IOBufferWithSize> buf = CreateTestDataBuffer();
  stream.Write(buf, buf->size(), callback.callback());

  // Close the stream without waiting for the completion.
  stream.CloseSync();
}

// TODO(satorux): This should be gone once all once all async clients are
// migrated to use Close(). crbug.com/114783
TEST_F(FileStreamTest, AsyncOpenAndCloseSync) {
  FileStream stream(NULL);
  int flags = base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream.Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Close the stream without waiting for the completion. Should be safe.
  stream.CloseSync();
  // open_callback won't be called.
  EXPECT_FALSE(open_callback.have_result());
}

TEST_F(FileStreamTest, AsyncOpenAndDelete) {
  scoped_ptr<FileStream> stream(new FileStream(NULL));
  int flags = base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream->Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Delete the stream without waiting for the open operation to be
  // complete. Should be safe.
  stream.reset();
  // open_callback won't be called.
  EXPECT_FALSE(open_callback.have_result());
}

TEST_F(FileStreamTest, AsyncCloseAndDelete) {
  scoped_ptr<FileStream> stream(new FileStream(NULL));
  int flags = base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC;
  TestCompletionCallback open_callback;
  int rv = stream->Open(temp_file_path(), flags, open_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, open_callback.WaitForResult());
  EXPECT_TRUE(stream->IsOpen());

  TestCompletionCallback close_callback;
  stream->Close(close_callback.callback());

  // Delete the stream without waiting for the close operation to be
  // complete. Should be safe.
  stream.reset();
  // close_callback won't be called.
  EXPECT_FALSE(close_callback.have_result());
}

}  // namespace

}  // namespace net
