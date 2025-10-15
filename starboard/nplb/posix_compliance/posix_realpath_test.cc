// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

/*
 The following error conditions are not tested due to the difficulty of
 reliably triggering them in a portable unit testing environment:
 - EACCES (permission errors)
 - EIO (I/O errors)
*/

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const mode_t kUserRwx = S_IRWXU;

// Test fixture for realpath tests. It sets up a temporary directory with a
// predefined structure of files, subdirectories, and symbolic links.
class PosixRealpathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char template_name[] = "/tmp/realpath_test_XXXXXX";
    char* dir_name = mkdtemp(template_name);
    ASSERT_NE(dir_name, nullptr) << "mkdtemp failed: " << strerror(errno);
    test_dir_ = dir_name;

    // Create a known file/directory structure for tests:
    // test_dir_
    // |-- file1
    // |-- subdir1
    // |   `-- file2
    //
    // (The files below are a part of the file/directory structure, but are
    //  only defined in the tests that need them).
    //
    // |-- symlink_to_file1 -> (absolute path to file1)
    // |-- symlink_to_subdir1 -> (absolute path to subdir1)
    // |-- dangling_symlink -> "non_existent"
    // |-- loop_a -> "loop_b"
    // `-- loop_b -> "loop_a"

    // Create file1
    std::string file1_path = test_dir_ + "/file1";
    int fd = open(file1_path.c_str(), O_CREAT | O_WRONLY, 0600);
    ASSERT_NE(fd, -1);
    ASSERT_EQ(close(fd), 0);

    // Create subdir1 and file2
    std::string subdir1_path = test_dir_ + "/subdir1";
    ASSERT_EQ(mkdir(subdir1_path.c_str(), kUserRwx), 0);
    std::string file2_path = subdir1_path + "/file2";
    fd = open(file2_path.c_str(), O_CREAT | O_WRONLY, 0600);
    ASSERT_NE(fd, -1);
    ASSERT_EQ(close(fd), 0);
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      RemoveFileOrDirectoryRecursively(test_dir_.c_str());
    }
  }

  std::string test_dir_;
};

TEST_F(PosixRealpathTest, ResolvesBasicPath) {
  std::string original_path = test_dir_ + "/file1";
  std::string expected_path = original_path;
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
  EXPECT_EQ(result, resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathWithDot) {
  std::string original_path = test_dir_ + "/./file1";
  std::string expected_path = test_dir_ + "/file1";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathWithDotDot) {
  std::string original_path = test_dir_ + "/subdir1/../file1";
  std::string expected_path = test_dir_ + "/file1";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathWithMultipleSlashes) {
  std::string original_path = test_dir_ + "///subdir1//file2";
  std::string expected_path = test_dir_ + "/subdir1/file2";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathWithTrailingSlash) {
  std::string original_path = test_dir_ + "/subdir1/";
  std::string expected_path = test_dir_ + "/subdir1";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesRootPath) {
  char resolved_path[PATH_MAX];
  errno = 0;
  char* result = realpath("/", resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ("/", resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathEndingInSymlink) {
  std::string file1_path = test_dir_ + "/file1";
  ASSERT_EQ(
      symlink(file1_path.c_str(), (test_dir_ + "/symlink_to_file1").c_str()),
      0);
  std::string original_path = test_dir_ + "/symlink_to_file1";
  std::string expected_path = test_dir_ + "/file1";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, ResolvesPathContainingSymlink) {
  std::string subdir1_path = test_dir_ + "/subdir1";
  ASSERT_EQ(symlink(subdir1_path.c_str(),
                    (test_dir_ + "/symlink_to_subdir1").c_str()),
            0);
  std::string original_path = test_dir_ + "/symlink_to_subdir1/file2";
  std::string expected_path = test_dir_ + "/subdir1/file2";
  char resolved_path[PATH_MAX];

  errno = 0;
  char* result = realpath(original_path.c_str(), resolved_path);
  ASSERT_NE(result, nullptr) << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(expected_path.c_str(), resolved_path);
}

TEST_F(PosixRealpathTest, FailsWithEmptyPath) {
  char resolved_path[PATH_MAX];
  errno = 0;
  EXPECT_EQ(realpath("", resolved_path), nullptr);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixRealpathTest, FailsWithNullFileName) {
  char resolved_path[PATH_MAX];
  errno = 0;
  EXPECT_EQ(realpath(nullptr, resolved_path), nullptr);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixRealpathTest, FailsIfComponentDoesNotExist) {
  std::string original_path = test_dir_ + "/non_existent/file";
  char resolved_path[PATH_MAX];

  errno = 0;
  EXPECT_EQ(realpath(original_path.c_str(), resolved_path), nullptr);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixRealpathTest, FailsWithDanglingSymlink) {
  ASSERT_EQ(symlink("non_existent", (test_dir_ + "/dangling_symlink").c_str()),
            0);
  std::string original_path = test_dir_ + "/dangling_symlink";
  char resolved_path[PATH_MAX];

  errno = 0;
  EXPECT_EQ(realpath(original_path.c_str(), resolved_path), nullptr);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixRealpathTest, FailsIfPathComponentIsNotDirectory) {
  std::string original_path = test_dir_ + "/file1/some_other_file";
  char resolved_path[PATH_MAX];

  errno = 0;
  EXPECT_EQ(realpath(original_path.c_str(), resolved_path), nullptr);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixRealpathTest, FailsWithSymbolicLinkLoop) {
  // Create a symlink loop. Targets are relative as they are in the same dir.
  ASSERT_EQ(symlink("loop_b", (test_dir_ + "/loop_a").c_str()), 0);
  ASSERT_EQ(symlink("loop_a", (test_dir_ + "/loop_b").c_str()), 0);
  std::string original_path = test_dir_ + "/loop_a";
  char resolved_path[PATH_MAX];

  errno = 0;
  EXPECT_EQ(realpath(original_path.c_str(), resolved_path), nullptr);
  EXPECT_EQ(errno, ELOOP);
}

TEST_F(PosixRealpathTest, FailsOnPathComponentTooLong) {
  std::string long_component(PATH_MAX, 'b');
  std::string long_path = test_dir_ + "/" + long_component;

  char resolved_path[PATH_MAX];
  errno = 0;
  EXPECT_EQ(realpath(long_path.c_str(), resolved_path), nullptr);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

}  // namespace
}  // namespace nplb
