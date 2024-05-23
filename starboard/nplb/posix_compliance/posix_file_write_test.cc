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
#include <unistd.h>

#include <string>

#include "starboard/common/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Sets up an empty test fixture, required for typed tests.
template <class SbFileWriteType>
class PosixFileWriteTest : public testing::Test {};

class PosixFileWriter {
 public:
  static ssize_t Write(int file, char* data, size_t size) {
    return write(file, data, size);
  }
};

class PosixFileWriterAll {
 public:
  static ssize_t Write(int file, char* data, size_t size) {
    return WriteAll(file, data, size);
  }
};

typedef testing::Types<PosixFileWriter, PosixFileWriterAll>
    PosixFileWriteTestTypes;

TYPED_TEST_CASE(PosixFileWriteTest, PosixFileWriteTestTypes);

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
  ASSERT_TRUE(file >= 0);

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
    int bytes_read = ReadAll(file, buffer, kBufferLength);
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
  ASSERT_TRUE(file >= 0);

  char buffer[kBufferLength] = {0};

  // Write zero bytes.
  for (int i = 0; i < 10; ++i) {
    int bytes_written = TypeParam::Write(file, buffer, 0);
    EXPECT_EQ(0, bytes_written);
  }

  int result = close(file);
  EXPECT_TRUE(result == 0);

  file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);
  struct stat info;
  result = fstat(file, &info);
  EXPECT_TRUE(result == 0);
  EXPECT_EQ(0, info.st_size);
  result = close(file);
  EXPECT_TRUE(result == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
