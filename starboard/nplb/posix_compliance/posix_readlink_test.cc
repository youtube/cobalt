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

// Untested Scenarios:
// 1. EFAULT: `pathname` or `buf` point to an invalid memory address outside the
//    process's address space. Triggering this deterministically is difficult
//    and risky in a standard test environment without causing a segfault.
// 2. EIO: An I/O error occurred while reading from the filesystem. This is
//    typically hardware-dependent and hard to simulate reliably.
// 3. ENOMEM: Insufficient kernel memory was available. This is not feasible
//    to trigger in a standard unit test.

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixReadlinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char template_name[] = "/tmp/readlink_test_XXXXXX";
    char* dir_name = mkdtemp(template_name);
    ASSERT_NE(dir_name, nullptr) << "mkdtemp failed: " << strerror(errno);
    test_dir_ = dir_name;

    link_target_content_ = "a/relative/path/to/a/file.txt";
    link_path_ = test_dir_ + "/the_link";

    int res = symlink(link_target_content_.c_str(), link_path_.c_str());
    ASSERT_EQ(res, 0) << "Failed to create symlink in setup: "
                      << strerror(errno);
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      ASSERT_TRUE(RemoveFileOrDirectoryRecursively(test_dir_.c_str()));
    }
  }

  std::string test_dir_;
  std::string link_path_;
  std::string link_target_content_;
};

TEST_F(PosixReadlinkTest, SunnyDay) {
  char buf[PATH_MAX];
  ssize_t len = readlink(link_path_.c_str(), buf, sizeof(buf));
  ASSERT_NE(len, -1);
  ASSERT_EQ(static_cast<size_t>(len), link_target_content_.length());
  ASSERT_EQ(std::string(buf, len), link_target_content_);
}

TEST_F(PosixReadlinkTest, Truncation) {
  char buf[10];
  ssize_t len = readlink(link_path_.c_str(), buf, sizeof(buf));
  ASSERT_NE(len, -1);
  ASSERT_EQ(static_cast<unsigned long>(len), sizeof(buf));
  ASSERT_EQ(std::string(buf, len), link_target_content_.substr(0, sizeof(buf)));
}

TEST_F(PosixReadlinkTest, DoesNotNullTerminate) {
  size_t content_len = link_target_content_.length();
  char buf[content_len + 5];
  memset(buf, 'X', sizeof(buf));

  ssize_t len = readlink(link_path_.c_str(), buf, content_len);

  ASSERT_NE(len, -1);
  ASSERT_EQ(static_cast<size_t>(len), content_len);
  ASSERT_EQ(std::string(buf, len), link_target_content_);
  // Check that the byte immediately after the content is untouched.
  ASSERT_EQ(buf[content_len], 'X');
}

TEST_F(PosixReadlinkTest, PathIsNotSymlinkFails) {
  std::string regular_file = test_dir_ + "/regular.txt";
  int fd = open(regular_file.c_str(), O_CREAT | O_WRONLY, kUserRw);
  ASSERT_NE(fd, -1) << "Failed to create test file: " << strerror(errno);
  ASSERT_EQ(write(fd, "hello", 5), 5);
  close(fd);

  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(regular_file.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixReadlinkTest, NonExistentPathFails) {
  std::string non_existent_path = test_dir_ + "/does_not_exist";
  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(non_existent_path.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixReadlinkTest, EmptyPathFails) {
  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink("", buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixReadlinkTest, PathComponentNotDirectoryFails) {
  std::string file_as_dir = test_dir_ + "/file_as_dir";
  int fd = open(file_as_dir.c_str(), O_CREAT | O_WRONLY, kUserRw);
  ASSERT_NE(fd, -1);
  close(fd);

  std::string path = file_as_dir + "/link_wannabe";
  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(path.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixReadlinkTest, PermissionDeniedFails) {
  // This test will not work correctly if run as root, as root bypasses
  // file permission checks.
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  constexpr mode_t user_rwx = S_IRUSR | S_IWUSR | S_IXUSR;
  std::string protected_dir = test_dir_ + "/protected";
  std::string link_in_protected = protected_dir + "/inner_link";

  ASSERT_EQ(mkdir(protected_dir.c_str(), user_rwx), 0);
  ASSERT_EQ(symlink("target", link_in_protected.c_str()), 0);

  // Remove search (execute) permission from a component of the path.
  ASSERT_EQ(chmod(protected_dir.c_str(), S_IRUSR | S_IWUSR), 0);

  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(link_in_protected.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, EACCES);

  // Restore permissions for cleanup.
  chmod(protected_dir.c_str(), user_rwx);
}

TEST_F(PosixReadlinkTest, InvalidBufferSizeFails) {
  char buf[1];

  errno = 0;
  EXPECT_EQ(readlink(link_path_.c_str(), buf, 0), -1);
  EXPECT_EQ(errno, EINVAL);

  errno = 0;
  EXPECT_EQ(readlink(link_path_.c_str(), buf, -1), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixReadlinkTest, SymlinkLoopFails) {
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";

  ASSERT_EQ(symlink(link_b_path.c_str(), link_a_path.c_str()), 0);
  ASSERT_EQ(symlink(link_a_path.c_str(), link_b_path.c_str()), 0);

  // readlink itself doesn't detect loops; it just reads the link content.
  // The loop error occurs when a path resolution function like `open` or
  // `stat` has to follow the links. `readlink` on a path that *contains*
  // a loop in its components will fail.
  std::string looped_path = link_a_path + "/some_file";
  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(looped_path.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, ELOOP);
}

TEST_F(PosixReadlinkTest, PathTooLongFails) {
  std::string long_name(PATH_MAX + 1, 'a');
  std::string long_path = test_dir_ + "/" + long_name;

  char buf[PATH_MAX];
  errno = 0;
  EXPECT_EQ(readlink(long_path.c_str(), buf, sizeof(buf)), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

}  // namespace
}  // namespace nplb
