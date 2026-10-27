// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Writing is partially tested by some of the file helpers that create files
// for the tests to operate on.

#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <string>

#include "starboard/common/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Sets up an empty test fixture, required for typed tests.
template <class SbFileWriteType>
class PosixFileWriteTest : public ::testing::Test {};

class PosixFileWriter {
 public:
  static ssize_t Write(int file, char* data, size_t size) {
    return write(file, data, size);
  }
};

class PosixFileWriterAll {
 public:
  static ssize_t Write(int file, char* data, size_t size) {
    return starboard::WriteAll(file, data, size);
  }
};

using PosixFileWriteTestTypes =
    ::testing::Types<PosixFileWriter, PosixFileWriterAll>;

TYPED_TEST_SUITE(PosixFileWriteTest, PosixFileWriteTestTypes);

const int kBufferLength = 16 * 1024;

TYPED_TEST(PosixFileWriteTest, InvalidFileErrors) {
  char buffer[kBufferLength] = {0};
  int result = TypeParam::Write(-1, buffer, kBufferLength);
  EXPECT_EQ(-1, result);
}

TYPED_TEST(PosixFileWriteTest, BasicWriting) {
  // Choose a file size that is not an even multiple of the buffer size, but
  // is over several times the size of the buffer.
  const int kFileSize = kBufferLength * 16 / 3;
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  // Create a bigger buffer than necessary, so we can test the memory around
  // the portion given to SbFileRead.
  char buffer[kBufferLength] = {0};

  // Initialize to some arbitrary pattern so we can verify it later.
  for (int i = 0; i < kBufferLength; ++i) {
    buffer[i] = static_cast<char>(i & 0xFF);
  }

  // Read and check the whole file.
  int total = 0;
  while (true) {
    if (total == kFileSize) {
      break;
    }

    int remaining = kFileSize - total;
    int to_write = remaining < kBufferLength ? remaining : kBufferLength;
    int bytes_written =
        TypeParam::Write(file, buffer + (total % kBufferLength), to_write);

    // Check that we didn't write more than the buffer size.
    EXPECT_GE(to_write, bytes_written);

    // Check that we didn't get an error.
    ASSERT_LT(0, bytes_written);

    total += bytes_written;
    EXPECT_EQ(total, lseek(file, 0, SEEK_CUR));
  }

  // Tests reading and writing from same opened file.
  int result = fsync(file);
  ASSERT_TRUE(result == 0);
  int position = static_cast<int>(lseek(file, 0, SEEK_SET));
  ASSERT_EQ(0, position);

  // Read and check the whole file.
  total = 0;
  int previous_total = 0;
  while (true) {
    int bytes_read = starboard::ReadAll(file, buffer, kBufferLength);
    if (bytes_read == 0) {
      break;
    }

    // Check that we didn't read more than the buffer size.
    EXPECT_GE(kBufferLength, bytes_read);

    // Check that we didn't get an error.
    ASSERT_LT(0, bytes_read);

    // Do some accounting to check later.
    previous_total = total;
    total += bytes_read;

    ScopedRandomFile::ExpectPattern(previous_total, buffer, bytes_read,
                                    __LINE__);
  }

  // Check that we read the whole file.
  EXPECT_EQ(kFileSize, total);

  result = close(file);
  EXPECT_TRUE(result == 0);
}

TYPED_TEST(PosixFileWriteTest, WriteZeroBytes) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  char buffer[kBufferLength] = {0};

  // Write zero bytes.
  for (int i = 0; i < 10; ++i) {
    int bytes_written = TypeParam::Write(file, buffer, 0);
    EXPECT_EQ(0, bytes_written);
  }

  int result = close(file);
  EXPECT_TRUE(result == 0);

  file = open(filename.c_str(), O_RDONLY);
  ASSERT_GE(file, 0) << strerror(errno);
  struct stat info{};
  result = fstat(file, &info);
  EXPECT_TRUE(result == 0);
  EXPECT_EQ(0, info.st_size);
  result = close(file);
  EXPECT_TRUE(result == 0);
}

TYPED_TEST(PosixFileWriteTest, BasicPwrite) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  const char write_data1[] = "Hello";
  const size_t write_size1 = sizeof(write_data1) - 1;
  const off_t offset1 = 0;

  ssize_t bytes_pwritten1 = pwrite(file, write_data1, write_size1, offset1);
  ASSERT_EQ(static_cast<ssize_t>(write_size1), bytes_pwritten1);
  EXPECT_EQ(0, lseek(file, 0, SEEK_CUR));  // Offset should not change

  const char write_data2[] = "World";
  const size_t write_size2 = sizeof(write_data2) - 1;
  const off_t offset2 = 6;

  ssize_t bytes_pwritten2 = pwrite(file, write_data2, write_size2, offset2);
  ASSERT_EQ(static_cast<ssize_t>(write_size2), bytes_pwritten2);
  EXPECT_EQ(0, lseek(file, 0, SEEK_CUR));  // Offset should not change

  char read_buffer[128] = {0};
  const ssize_t bytes_read =
      pread(file, read_buffer, sizeof(read_buffer) - 1, 0);
  ASSERT_GE(bytes_read, static_cast<ssize_t>(offset2 + write_size2));
  EXPECT_STREQ("Hello\0World", read_buffer);  // Expect null termination after
                                              // "Hello" due to initial zeroing

  EXPECT_EQ(close(file), 0);
}

TYPED_TEST(PosixFileWriteTest, PwriteBeyondEndOfFile) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  const char write_data[] = "Test";
  const size_t write_size = sizeof(write_data) - 1;
  const off_t offset = 10;  // Write beyond the initial empty file

  ssize_t bytes_pwritten = pwrite(file, write_data, write_size, offset);
  ASSERT_EQ(static_cast<ssize_t>(write_size), bytes_pwritten);
  EXPECT_EQ(0, lseek(file, 0, SEEK_CUR));  // Offset should not change

  struct stat info{};
  ASSERT_EQ(0, fstat(file, &info));
  EXPECT_EQ(static_cast<off_t>(offset + write_size),
            info.st_size);  // File should be extended

  char read_buffer[128] = {0};
  const ssize_t bytes_read =
      pread(file, read_buffer, sizeof(read_buffer) - 1, 0);
  ASSERT_EQ(static_cast<ssize_t>(offset + write_size), bytes_read);
  EXPECT_EQ(
      '\0',
      read_buffer[0]);  // Bytes before the written data should be zero-filled
  EXPECT_EQ('\0', read_buffer[9]);
  EXPECT_STREQ("Test", read_buffer + 10);

  EXPECT_EQ(close(file), 0);
}

TYPED_TEST(PosixFileWriteTest, PwriteAtDifferentOffsets) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  const char data1[] = "First";
  EXPECT_NE(pwrite(file, data1, sizeof(data1) - 1, 0), -1);

  const char data2[] = "Second";
  EXPECT_NE(pwrite(file, data2, sizeof(data2) - 1, 10), -1);

  const char data3[] = "Third";
  EXPECT_NE(pwrite(file, data3, sizeof(data3) - 1, 5), -1);

  char read_buffer[128] = {0};
  EXPECT_NE(pread(file, read_buffer, sizeof(read_buffer) - 1, 0), -1);
  EXPECT_STREQ("FirstThirdSecond", read_buffer);

  EXPECT_EQ(close(file), 0);
}

TYPED_TEST(PosixFileWriteTest, PwriteZeroLength) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  const char data[] = "SomeData";
  EXPECT_NE(pwrite(file, data, sizeof(data) - 1, 5), -1);

  ssize_t bytes_pwritten = pwrite(file, nullptr, 0, 2);
  EXPECT_EQ(0, bytes_pwritten);
  EXPECT_EQ(0, lseek(file, 0, SEEK_CUR));  // Offset should not change

  char read_buffer[16] = {0};
  EXPECT_NE(pread(file, read_buffer, sizeof(read_buffer) - 1, 0), -1);
  EXPECT_EQ('\0', read_buffer[0]);
  EXPECT_EQ('\0', read_buffer[1]);
  EXPECT_EQ(
      '\0',
      read_buffer[2]);  // Writing zero bytes shouldn't change existing data
  EXPECT_EQ('\0', read_buffer[3]);
  EXPECT_EQ('\0', read_buffer[4]);
  EXPECT_STREQ("SomeData", read_buffer + 5);

  EXPECT_EQ(close(file), 0);
}

TYPED_TEST(PosixFileWriteTest, BasicWritev) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
  const int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  std::string data1 = "Hello, ";
  std::string data2 = "World!";
  std::string data3 = " 123";

  const struct iovec iov[3] = {
      {.iov_base = const_cast<char*>(data1.c_str()), .iov_len = data1.size()},
      {.iov_base = const_cast<char*>(data2.c_str()), .iov_len = data2.size()},
      {.iov_base = const_cast<char*>(data3.c_str()), .iov_len = data3.size()}};

  const ssize_t bytes_written = writev(file, iov, 3);
  EXPECT_NE(bytes_written, -1) << "writev() failed: " << strerror(errno);
  ASSERT_EQ(static_cast<ssize_t>(data1.size() + data2.size() + data3.size()),
            bytes_written);

  char read_buffer[20] = {0};
  const ssize_t bytes_read =
      pread(file, read_buffer, sizeof(read_buffer) - 1, 0);
  ASSERT_EQ(bytes_written, bytes_read) << strerror(errno);
  EXPECT_STREQ("Hello, World! 123", read_buffer);

  EXPECT_EQ(close(file), 0) << "close failed: " << strerror(errno);
}

TYPED_TEST(PosixFileWriteTest, WritevPartial) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
  const int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  std::string data1 = "First";
  std::string data2 = "Second";
  std::string data3 = "Third";

  const struct iovec iov[3] = {
      {.iov_base = const_cast<char*>(data1.c_str()), .iov_len = data1.size()},
      {.iov_base = const_cast<char*>(data2.c_str()), .iov_len = data2.size()},
      {.iov_base = const_cast<char*>(data3.c_str()), .iov_len = data3.size()}};

  const ssize_t bytes_written =
      writev(file, iov, 2);  // Write only the first two.
  EXPECT_NE(bytes_written, -1) << "writev() failed: " << strerror(errno);
  ASSERT_EQ(static_cast<ssize_t>(data1.size() + data2.size()), bytes_written);

  char read_buffer[20] = {0};
  const ssize_t bytes_read =
      pread(file, read_buffer, sizeof(read_buffer) - 1, 0);
  ASSERT_EQ(bytes_written, bytes_read) << strerror(errno);
  EXPECT_STREQ("FirstSecond", read_buffer);

  EXPECT_EQ(close(file), 0) << "close failed: " << strerror(errno);
}

TYPED_TEST(PosixFileWriteTest, WritevEmpty) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
  const int file =
      open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  ASSERT_GE(file, 0) << strerror(errno);

  struct iovec iov[1];
  iov[0].iov_base = nullptr;
  iov[0].iov_len = 0;

  const ssize_t bytes_written = writev(file, iov, 1);
  EXPECT_NE(bytes_written, -1) << "writev() failed: " << strerror(errno);
  EXPECT_EQ(0, bytes_written);

  struct stat info{};
  ASSERT_EQ(0, fstat(file, &info)) << strerror(errno);
  EXPECT_EQ(0, info.st_size);

  EXPECT_EQ(close(file), 0) << "close failed: " << strerror(errno);
}
}  // namespace
}  // namespace nplb
