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

#if SB_API_VERSION >= 16

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/file.h"
#include "starboard/nplb/posix_compliance/posix_file_helpers.h"
#include "starboard/shared/posix/file_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void BasicTest(bool existing,
               int open_flags,
               bool expected_created,
               bool expected_success,
               int original_line) {
  ScopedRandomFile random_file(existing ? ScopedRandomFile::kCreate
                                        : ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
#define SB_FILE_OPEN_TEST_CONTEXT                                      \
  "existing=" << existing << ", flags=0x" << std::hex << open_flags    \
              << std::dec << ", expected_created=" << expected_created \
              << ", expected_success=" << expected_success             \
              << ", filename=" << filename                             \
              << ", original_line=" << original_line

  if (!existing) {
    EXPECT_FALSE(SbFileExists(filename.c_str())) << SB_FILE_OPEN_TEST_CONTEXT;
    if (SbFileExists(filename.c_str())) {
      return;
    }
  }

  bool created = !expected_created;
  int fd = open(filename.c_str(), open_flags);
  if (!expected_success) {
    EXPECT_FALSE(fd >= 0) << SB_FILE_OPEN_TEST_CONTEXT;
    // EXPECT_EQ(expected_created, created) << SB_FILE_OPEN_TEST_CONTEXT;
    // EXPECT_NE(kSbFileOk, error) << SB_FILE_OPEN_TEST_CONTEXT;

    // Try to clean up in case test fails.
    if (!(fd < 0)) {
      close(fd);
    }
  } else {
    EXPECT_TRUE(fd >= 0);
    // EXPECT_EQ(expected_created, created) << SB_FILE_OPEN_TEST_CONTEXT;
    // EXPECT_EQ(kSbFileOk, error) << SB_FILE_OPEN_TEST_CONTEXT;
    if (fd >= 0) {
      int result = close(fd);
      EXPECT_TRUE(result == 0) << SB_FILE_OPEN_TEST_CONTEXT;
    }
  }
#undef SB_FILE_OPEN_TEST_CONTEXT
}

TEST(PosixFileOpenTest, OpenOnlyOpensExistingFile) {
  // BasicTest(true, kSbFileOpenOnly | kSbFileRead, false, true, __LINE__);
  BasicTest(true, O_RDONLY, false, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenOnlyDoesNotOpenNonExistingFile) {
  // BasicTest(false, kSbFileOpenOnly | kSbFileRead, false, false, __LINE__);
  BasicTest(false, O_RDONLY, false, false, __LINE__);
}

TEST(PosixFileOpenTest, CreateOnlyDoesNotCreateExistingFile) {
  // BasicTest(true, kSbFileCreateOnly | kSbFileWrite, false, false, __LINE__);
  BasicTest(true, O_CREAT | O_EXCL | O_WRONLY, false, false, __LINE__);
}

TEST(PosixFileOpenTest, CreateOnlyCreatesNonExistingFile) {
  // BasicTest(false, kSbFileCreateOnly | kSbFileWrite, true, true, __LINE__);
  BasicTest(false, O_CREAT | O_EXCL | O_WRONLY, true, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenAlwaysOpensExistingFile) {
  // BasicTest(true, kSbFileOpenAlways | kSbFileWrite, false, true, __LINE__);
  BasicTest(true, O_CREAT | O_WRONLY, false, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenAlwaysCreatesNonExistingFile) {
  BasicTest(false, kSbFileOpenAlways | kSbFileWrite, true, true, __LINE__);
  BasicTest(false, O_CREAT | O_WRONLY, true, true, __LINE__);
}

TEST(PosixFileOpenTest, CreateAlwaysTruncatesExistingFile) {
  // BasicTest(true, kSbFileCreateAlways | kSbFileWrite, true, true, __LINE__);
  BasicTest(true, O_CREAT | O_TRUNC | O_WRONLY, true, true, __LINE__);
}

TEST(PosixFileOpenTest, CreateAlwaysCreatesNonExistingFile) {
  // BasicTest(false, kSbFileCreateAlways | kSbFileWrite, true, true, __LINE__);
  BasicTest(false, O_CREAT | O_TRUNC | O_WRONLY, true, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenTruncatedTruncatesExistingFile) {
  // BasicTest(true, kSbFileOpenTruncated | kSbFileWrite, false, true,
  // __LINE__);
  BasicTest(true, O_TRUNC | O_WRONLY, false, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenTruncatedDoesNotCreateNonExistingFile) {
  // BasicTest(false, kSbFileOpenTruncated | kSbFileWrite, false, false,
  // __LINE__);
  BasicTest(false, O_TRUNC | O_WRONLY, false, false, __LINE__);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION >= 16
