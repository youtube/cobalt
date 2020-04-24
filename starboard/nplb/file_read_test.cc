// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Sets up an empty test fixture, required for typed tests.
template <class SbFileReadType>
class SbFileReadTest : public testing::Test {};

class SbFileReader {
 public:
  static int Read(SbFile file, char* data, int size) {
    return SbFileRead(file, data, size);
  }
};

class SbFileReaderAll {
 public:
  static int Read(SbFile file, char* data, int size) {
    return SbFileReadAll(file, data, size);
  }
};

typedef testing::Types<SbFileReader, SbFileReaderAll> SbFileReadTestTypes;

TYPED_TEST_CASE(SbFileReadTest, SbFileReadTestTypes);

const int kBufferLength = 16 * 1024;

TYPED_TEST(SbFileReadTest, InvalidFileErrors) {
  char buffer[kBufferLength];
  int result = TypeParam::Read(kSbFileInvalid, buffer, kBufferLength);
  EXPECT_EQ(-1, result);
}

TYPED_TEST(SbFileReadTest, BasicReading) {
  // Create a pattern file that is not an even multiple of the buffer size,
  // but is over several times the size of the buffer.
  const int kFileSize = kBufferLength * 16 / 3;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

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

  bool result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TYPED_TEST(SbFileReadTest, ReadPastEnd) {
  const int kFileSize = kBufferLength;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

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

  // Read off the end of the file.
  int position = static_cast<int>(SbFileSeek(file, kSbFileFromEnd, 0));
  EXPECT_EQ(kFileSize, position);
  int bytes_read = TypeParam::Read(file, buffer, kBufferLength);
  EXPECT_EQ(0, bytes_read);

  for (int i = 0; i < kRealBufferLength; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  bool result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TYPED_TEST(SbFileReadTest, ReadZeroBytes) {
  const int kFileSize = kBufferLength;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

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

  bool result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TYPED_TEST(SbFileReadTest, ReadFromMiddle) {
  const int kFileSize = kBufferLength * 2;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();

  SbFile file =
      SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  ASSERT_TRUE(SbFileIsValid(file));

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
  int position =
      static_cast<int>(SbFileSeek(file, kSbFileFromBegin, kFileSize / 4));
  EXPECT_EQ(kFileSize / 4, position);
  int bytes_read = TypeParam::Read(file, buffer, kBufferLength);
  EXPECT_GE(kBufferLength, bytes_read);
  EXPECT_LT(0, bytes_read);

  ScopedRandomFile::ExpectPattern(position, buffer, bytes_read, __LINE__);

  for (int i = 0; i < kBufferOffset; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  for (int i = kBufferOffset + bytes_read; i < kRealBufferLength; ++i) {
    EXPECT_EQ('\xCD', real_buffer[i]);
  }

  bool result = SbFileClose(file);
  EXPECT_TRUE(result);
}

TYPED_TEST(SbFileReadTest, ReadStaticContent) {
  for (auto filename : GetFileTestsFilePaths()) {
    SbFile file =
        SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    ASSERT_TRUE(SbFileIsValid(file)) << "Can't open: " << filename;

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

    bool result = SbFileClose(file);
    EXPECT_TRUE(result);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
