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

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixOpenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> temp_path(kSbFileMaxPath);
    bool success = SbSystemGetPath(kSbSystemPathTempDirectory, temp_path.data(),
                                   kSbFileMaxPath);
    ASSERT_TRUE(success);

    std::string path_template =
        std::string(temp_path.data()) + "/open_test.XXXXXX";
    std::vector<char> buffer(path_template.begin(), path_template.end());
    buffer.push_back('\0');

    char* created_path = mkdtemp(buffer.data());
    ASSERT_TRUE(created_path != nullptr);
    test_dir_ = created_path;
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      RemoveFileOrDirectoryRecursively(test_dir_);
    }
  }

  std::string test_dir_;
};

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

TEST_F(PosixOpenTest, SuccessReadWrite) {
  std::string file_path = test_dir_ + "/test.txt";
  int fd = open(file_path.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);

  // Write, seek, then read to verify.
  const char* write_buf = "hello";
  char read_buf[6] = {0};
  ASSERT_EQ(write(fd, write_buf, 5), 5);
  ASSERT_NE(lseek(fd, 0, SEEK_SET), -1);
  ASSERT_EQ(read(fd, read_buf, 5), 5);
  EXPECT_STREQ(write_buf, read_buf);

  close(fd);
}

TEST_F(PosixOpenTest, SuccessAppend) {
  std::string file_path = test_dir_ + "/test.txt";
  int fd = open(file_path.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(write(fd, "initial", 7), 7);
  close(fd);

  // Re-open with append flag.
  fd = open(file_path.c_str(), O_APPEND | O_WRONLY);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(write(fd, "_appended", 9), 9);
  close(fd);

  // Verify file content.
  std::vector<char> content(17);
  fd = open(file_path.c_str(), O_RDONLY);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(read(fd, content.data(), 16), 16);
  content[16] = '\0';
  EXPECT_STREQ("initial_appended", content.data());
  close(fd);
}

TEST_F(PosixOpenTest, FailsWithDirectoryOnRegularFile) {
  std::string file_path = test_dir_ + "/test.txt";
  int fd = open(file_path.c_str(), O_CREAT, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);
  close(fd);

  errno = 0;
  fd = open(file_path.c_str(), O_RDONLY | O_DIRECTORY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixOpenTest, FailsWithNoFollowOnSymlink) {
  std::string target_path = test_dir_ + "/target";
  int fd = open(target_path.c_str(), O_CREAT, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);
  close(fd);

  std::string link_path = test_dir_ + "/link";
  ASSERT_EQ(symlink(target_path.c_str(), link_path.c_str()), 0);

  errno = 0;
  fd = open(link_path.c_str(), O_RDONLY | O_NOFOLLOW);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ELOOP);
}

TEST_F(PosixOpenTest, FailsWithAccessDenied) {
  std::string file_path = test_dir_ + "/test.txt";
  int fd = open(file_path.c_str(), O_CREAT, S_IWUSR);  // Write-only
  ASSERT_NE(fd, -1);
  close(fd);

  errno = 0;
  fd = open(file_path.c_str(), O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EACCES);
}

TEST_F(PosixOpenTest, FailsIfIsDirectory) {
  errno = 0;
  int fd = open(test_dir_.c_str(), O_WRONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EISDIR);
}

TEST_F(PosixOpenTest, FailsWithSymbolicLinkLoop) {
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";
  ASSERT_EQ(symlink(link_b_path.c_str(), link_a_path.c_str()), 0);
  ASSERT_EQ(symlink(link_a_path.c_str(), link_b_path.c_str()), 0);

  std::string looping_path = link_a_path + "/foo";
  errno = 0;
  int fd = open(looping_path.c_str(), O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ELOOP);
}

TEST_F(PosixOpenTest, FailsWithNameTooLong) {
  std::string long_name(kSbFileMaxPath + 1, 'c');
  std::string long_path = test_dir_ + "/" + long_name;

  errno = 0;
  int fd = open(long_name.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST_F(PosixOpenTest, FailsWithPathComponentNotDirectory) {
  std::string file_path = test_dir_ + "/test.txt";
  int fd = open(file_path.c_str(), O_CREAT, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);
  close(fd);

  std::string invalid_path = file_path + "/another_file";
  errno = 0;
  fd = open(invalid_path.c_str(), O_RDONLY);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOTDIR);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
