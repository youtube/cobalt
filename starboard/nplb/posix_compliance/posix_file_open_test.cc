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
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void BasicTest(bool existing,
               int open_flags,
               bool expected_success,
               int original_line,
               mode_t mode = S_IRUSR | S_IWUSR) {
  ScopedRandomFile random_file(existing ? ScopedRandomFile::kCreate
                                        : ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();
#define SB_FILE_OPEN_TEST_CONTEXT                                      \
  "existing=" << existing << ", flags=0x" << std::hex << open_flags    \
              << std::dec << ", expected_success=" << expected_success \
              << ", filename=" << filename                             \
              << ", original_line=" << original_line

  if (!existing) {
    struct stat file_info;
    bool file_exists = stat(filename.c_str(), &file_info) == 0;
    EXPECT_FALSE(file_exists) << SB_FILE_OPEN_TEST_CONTEXT;
    if (file_exists) {
      return;
    }
  }

  int fd;
  fd = (open_flags & O_CREAT) ? open(filename.c_str(), open_flags, mode)
                              : open(filename.c_str(), open_flags);

  if (!expected_success) {
    EXPECT_FALSE(fd >= 0) << SB_FILE_OPEN_TEST_CONTEXT;

    // Try to clean up in case test fails.
    if (!(fd < 0)) {
      close(fd);
    }
  } else {
    EXPECT_TRUE(fd >= 0);
    if (fd >= 0) {
      int result = close(fd);
      EXPECT_TRUE(result == 0) << SB_FILE_OPEN_TEST_CONTEXT;
    }
  }
#undef SB_FILE_OPEN_TEST_CONTEXT
}

TEST(PosixFileOpenTest, OpenOnlyOpensExistingFile) {
  BasicTest(true, O_RDONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenOnlyDoesNotOpenNonExistingFile) {
  BasicTest(false, O_RDONLY, false, __LINE__);
}

TEST(PosixFileOpenTest, CreateOnlyDoesNotCreateExistingFile) {
  BasicTest(true, O_CREAT | O_EXCL | O_WRONLY, false, __LINE__);
}

TEST(PosixFileOpenTest, CreateOnlyCreatesNonExistingFile) {
  BasicTest(false, O_CREAT | O_EXCL | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenAlwaysOpensExistingFile) {
  BasicTest(true, O_CREAT | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenAlwaysCreatesNonExistingFile) {
  BasicTest(false, O_CREAT | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenAlwaysWithLinuxSpecificMode) {
  BasicTest(false, O_CREAT | O_TRUNC | O_WRONLY, true, __LINE__,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

TEST(PosixFileOpenTest, CreateAlwaysTruncatesExistingFile) {
  BasicTest(true, O_CREAT | O_TRUNC | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, CreateAlwaysCreatesNonExistingFile) {
  BasicTest(false, O_CREAT | O_TRUNC | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenTruncatedTruncatesExistingFile) {
  BasicTest(true, O_TRUNC | O_WRONLY, true, __LINE__);
}

TEST(PosixFileOpenTest, OpenTruncatedDoesNotCreateNonExistingFile) {
  BasicTest(false, O_TRUNC | O_WRONLY, false, __LINE__);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
