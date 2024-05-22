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

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Sets up an empty test fixture, required for typed tests.
template <class SbFileReadType>
class PosixFileReadTest : public testing::Test {};

class PosixRead {
 public:
  static ssize_t Read(int file, void* buf, size_t nbyte) {
    return read(file, buf, nbyte);
  }
};

class PosixReadAll {
 public:
  static ssize_t Read(int file, void* data, size_t size) {
    return ReadAll(file, data, size);
  }
};

typedef testing::Types<PosixRead, PosixReadAll> PosixFileReadTestTypes;

template <typename T, size_t n>
size_t array_size(const T (&)[n]) {
  return n;
}

TYPED_TEST_CASE(PosixFileReadTest, PosixFileReadTestTypes);

const int kBufferLength = 16 * 1024;

TYPED_TEST(PosixFileReadTest, InvalidFileErrors) {
  char buffer[kBufferLength];
  int result = TypeParam::Read(-1, buffer, kBufferLength);
  EXPECT_EQ(-1, result);
}

TYPED_TEST(PosixFileReadTest, BasicReading) {
  // Create a pattern file that is not an even multiple of the buffer size,
  // but is over several times the size of the buffer.
  const int kFileSize = kBufferLength * 16 / 3;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  // Create a bigger buffer than necessary, so we can test the memory around
  // the portion given to SbFileRead.
  const int kRealBufferLength = kBufferLength * 2;
  char real_buffer[kRealBufferLength] = {0};
  const int kBufferOffset = kBufferLength / 2;
  char* buffer = real_buffer + kBufferOffset;

  // Initialize to some arbitrary pattern so we can verify it later.
  for (int i = 0; i < kRealBufferLength; ++i) {
    real_buffer[i] = '\xCD';
  }

  // Read and check the whole file.
  int total = 0;
  int previous_total = 0;
  int max = 0;
  while (true) {
    int bytes_read = TypeParam::Read(file, buffer, kBufferLength);
    if (bytes_read == 0) {
      break;
    }

    // Check that we didn't read more than the buffer size.
    EXPECT_GE(kBufferLength, bytes_read);

    // Check that we didn't get an error.
    EXPECT_LT(0, bytes_read);

    // Do some accounting to check later.
    previous_total = total;
    total += bytes_read;
    if (bytes_read > max) {
      max = bytes_read;
    }

    ScopedRandomFile::ExpectPattern(previous_total, buffer, bytes_read,
                                    __LINE__);
  }

  // Check that we read the whole file.
  EXPECT_EQ(kFileSize, total);

  // check that we didn't write over any other parts of the buffer.
  for (int i = 0; i < kBufferOffset; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  for (int i = kBufferOffset + max; i < kRealBufferLength; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  bool result = close(file);
  EXPECT_TRUE(result == 0);
}

TYPED_TEST(PosixFileReadTest, ReadZeroBytes) {
  const int kFileSize = kBufferLength;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  // Create a bigger buffer than necessary, so we can test the memory around
  // the portion given to SbFileRead.
  const int kRealBufferLength = kBufferLength * 2;
  char real_buffer[kRealBufferLength] = {0};
  const int kBufferOffset = kBufferLength / 2;
  char* buffer = real_buffer + kBufferOffset;

  // Initialize to some arbitrary pattern so we can verify it later.
  for (int i = 0; i < kRealBufferLength; ++i) {
    real_buffer[i] = '\xCD';
  }

  // Read zero bytes.
  for (int i = 0; i < 10; ++i) {
    int bytes_read = TypeParam::Read(file, buffer, 0);
    EXPECT_EQ(0, bytes_read);
  }

  for (int i = 0; i < kRealBufferLength; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  int result = close(file);
  EXPECT_TRUE(result == 0);
}

TYPED_TEST(PosixFileReadTest, ReadFromMiddle) {
  const int kFileSize = kBufferLength * 2;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  int file = open(filename.c_str(), O_RDONLY);
  ASSERT_TRUE(file >= 0);

  // Create a bigger buffer than necessary, so we can test the memory around
  // the portion given to SbFileRead.
  const int kRealBufferLength = kBufferLength * 2;
  char real_buffer[kRealBufferLength] = {0};
  const int kBufferOffset = kBufferLength / 2;
  char* buffer = real_buffer + kBufferOffset;

  // Initialize to some arbitrary pattern so we can verify it later.
  for (int i = 0; i < kRealBufferLength; ++i) {
    real_buffer[i] = '\xCD';
  }

  // Read from the middle of the file.
  int position = static_cast<int>(lseek(file, kFileSize / 4, SEEK_SET));
  EXPECT_EQ(kFileSize / 4, position);
  int bytes_read = TypeParam::Read(file, buffer, kBufferLength);
  EXPECT_GE(kBufferLength, bytes_read);
  EXPECT_LT(0, bytes_read);

  ScopedRandomFile::ExpectPattern(position, buffer, bytes_read, __LINE__);

  for (int i = 0; i < kBufferOffset; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
    if ('\xCD' != real_buffer[i])
      break;
  }

  for (int i = kBufferOffset + bytes_read; i < kRealBufferLength; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
    if ('\xCD' != real_buffer[i])
      break;
  }

  int result = close(file);
  EXPECT_TRUE(result == 0);
}

TYPED_TEST(PosixFileReadTest, ReadStaticContent) {
  for (auto filename : GetFileTestsFilePaths()) {
    int file = open(filename.c_str(), O_RDONLY);
    ASSERT_TRUE(file >= 0) << "Can't open: " << filename;

    // Create a bigger buffer than necessary, so we can test the memory around
    // the portion given to SbFileRead.
    const int kRealBufferLength = kBufferLength * 2;
    char real_buffer[kRealBufferLength] = {0};
    const int kBufferOffset = kBufferLength / 2;
    char* buffer = real_buffer + kBufferOffset;

    // Initialize to some arbitrary pattern so we can verify it later.
    for (int i = 0; i < kRealBufferLength; ++i) {
      real_buffer[i] = '\xCD';
    }

    // Read and check the whole file.
    std::string content;
    int total = 0;
    int max = 0;
    while (true) {
      int bytes_read = TypeParam::Read(file, buffer, kBufferLength);
      if (bytes_read == 0) {
        break;
      }

      // Check that we didn't read more than the buffer size.
      EXPECT_GE(kBufferLength, bytes_read);

      // Check that we didn't get an error.
      EXPECT_LT(0, bytes_read);

      // Do some accounting to check later.
      total += bytes_read;
      if (bytes_read > max) {
        max = bytes_read;
      }

      // Accumulate the content of the whole file.
      content.append(buffer, bytes_read);
    }

    // Check that we didn't write over any other parts of the buffer.
    for (int i = 0; i < kBufferOffset; ++i) {
      EXPECT_EQ('\xCD', real_buffer[i]);
    }

    for (int i = kBufferOffset + max; i < kRealBufferLength; ++i) {
      EXPECT_EQ('\xCD', real_buffer[i]);
    }

    EXPECT_EQ(GetTestFileExpectedContent(filename), content);

    int result = close(file);
    EXPECT_TRUE(result == 0);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
