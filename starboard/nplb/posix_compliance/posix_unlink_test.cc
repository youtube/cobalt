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
  The following unlink() error conditions are not tested due to the difficulty
  of reliably triggering them in a portable unit testing environment:

  - [EBUSY]: The file is in use by the system or another process.
  - [EROFS]: The file is on a read-only file system.
  - [ETXTBSY]: The file is a currently executing pure procedure.
*/

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

class PosixUnlinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> temp_path(kSbFileMaxPath);
    bool success = SbSystemGetPath(kSbSystemPathTempDirectory, temp_path.data(),
                                   kSbFileMaxPath);
    ASSERT_TRUE(success);

    std::string path_template =
        std::string(temp_path.data()) + "/unlink_test.XXXXXX";
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

  bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
  }

  std::string test_dir_;
};

TEST_F(PosixUnlinkTest, SuccessRemovesFile) {
  std::string file_path = test_dir_ + "/test_file.txt";
  int fd = open(file_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  ASSERT_TRUE(FileExists(file_path));
  ASSERT_EQ(unlink(file_path.c_str()), 0)
      << "unlink failed with error " << strerror(errno);
  EXPECT_FALSE(FileExists(file_path));
}

TEST_F(PosixUnlinkTest, SuccessRemovesSymbolicLinkNotTarget) {
  std::string target_path = test_dir_ + "/target.txt";
  int fd = open(target_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  std::string link_path = test_dir_ + "/link.txt";
  ASSERT_EQ(symlink(target_path.c_str(), link_path.c_str()), 0)
      << "symlink failed with error " << strerror(errno);

  ASSERT_TRUE(FileExists(link_path));
  ASSERT_EQ(unlink(link_path.c_str()), 0)
      << "unlink failed with error " << strerror(errno);
  EXPECT_FALSE(FileExists(link_path));
  EXPECT_TRUE(FileExists(target_path));
}

TEST_F(PosixUnlinkTest, FailsOnDirectory) {
  std::string dir_path = test_dir_ + "/subdir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0)
      << "mkdir failed with error " << strerror(errno);

  errno = 0;
  EXPECT_EQ(unlink(dir_path.c_str()), -1);
  // posix expects eperm, linux expects eisdir
  EXPECT_TRUE(errno == EPERM || errno == EISDIR);
}

TEST_F(PosixUnlinkTest, FailsWithNoEntry) {
  std::string non_existent_path = test_dir_ + "/no_such_file.txt";
  errno = 0;
  EXPECT_EQ(unlink(non_existent_path.c_str()), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixUnlinkTest, FailsWithPathComponentNotDirectory) {
  std::string file_path = test_dir_ + "/file.txt";
  int fd = open(file_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  std::string invalid_path = file_path + "/another_file";
  errno = 0;
  EXPECT_EQ(unlink(invalid_path.c_str()), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixUnlinkTest, FailsWithPermissionDenied) {
  // This test will not work correctly if run as root, as root bypasses
  // file permission checks.
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  std::string file_path = test_dir_ + "/test_file.txt";
  int fd = open(file_path.c_str(), O_CREAT, kUserRw);
  ASSERT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  // Remove write permission from the parent directory.
  ASSERT_EQ(chmod(test_dir_.c_str(), S_IRUSR | S_IXUSR), 0);

  errno = 0;
  EXPECT_EQ(unlink(file_path.c_str()), -1);
  EXPECT_EQ(errno, EACCES);

  // Restore permissions for cleanup.
  chmod(test_dir_.c_str(), kUserRwx);
}

TEST_F(PosixUnlinkTest, FailsWithNameTooLong) {
  std::string long_name(kSbFileMaxPath + 1, 'a');
  std::string file_path = test_dir_ + "/" + long_name;

  errno = 0;
  EXPECT_EQ(unlink(file_path.c_str()), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST_F(PosixUnlinkTest, FailsWithSymbolicLinkLoop) {
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";
  ASSERT_EQ(symlink(link_b_path.c_str(), link_a_path.c_str()), 0);
  ASSERT_EQ(symlink(link_a_path.c_str(), link_b_path.c_str()), 0);

  std::string looping_path = link_a_path + "/some_file";
  errno = 0;
  EXPECT_EQ(unlink(looping_path.c_str()), -1);
  EXPECT_EQ(errno, ELOOP);
}

}  // namespace
}  // namespace starboard::nplb
