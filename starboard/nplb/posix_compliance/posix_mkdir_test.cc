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
  - EILSEQ (invalid byte sequence in path)
  - EIO (I/O errors)
  - EMLINK (link count exceeds {LINK_MAX})
  - ENOSPC (out-of-space/inode errors)
  - EROFS (read-only filesystem errors)
*/

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixMkdirTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char template_name[] = "/tmp/mkdir_test_XXXXXX";
    char* dir_name = mkdtemp(template_name);
    ASSERT_NE(dir_name, nullptr) << "mkdtemp failed: " << strerror(errno);
    test_dir_ = dir_name;
  }

  void TearDown() override {
    if (!test_dir_.empty()) {
      RemoveFileOrDirectoryRecursively(test_dir_.c_str());
    }
  }

  std::string test_dir_;
};

TEST_F(PosixMkdirTest, FailsWithEmptyPath) {
  // An empty path should result in a "No such file or directory" error.
  EXPECT_EQ(mkdir("", kUserRwx), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixMkdirTest, SuccessfulCreation) {
  std::string dir_path = test_dir_ + "/new_dir";
  const mode_t mode = kUserRwx;

  ASSERT_EQ(mkdir(dir_path.c_str(), mode), 0);

  // Verify that the directory was created with the correct properties.
  struct stat sb;
  ASSERT_EQ(stat(dir_path.c_str(), &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));

  // Verify user ID matches the process's effective user ID.
  EXPECT_EQ(sb.st_uid, geteuid());

  // The requested mode is masked by the process's umask. We can only check
  // that the resulting permissions are a subset of the requested ones.
  EXPECT_EQ((sb.st_mode & ~S_IFMT) & mode, (sb.st_mode & ~S_IFMT));

  // Verify the directory is empty (contains only "." and "..").
  DIR* dirp = opendir(dir_path.c_str());
  ASSERT_NE(dirp, nullptr);
  int entry_count = 0;
  errno = 0;
  while (readdir(dirp)) {
    entry_count++;
  }
  EXPECT_EQ(errno, 0) << "readdir failed";
  EXPECT_EQ(entry_count, 2);  // Should only contain "." and ".."
  closedir(dirp);
}

TEST_F(PosixMkdirTest, FailsOnEmptyPath) {
  errno = 0;
  EXPECT_EQ(mkdir("", kUserRwx), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixMkdirTest, HandlesTrailingSeparators) {
  std::string dir_base = test_dir_ + "/new_dir";
  std::string path_with_separators = dir_base + "//";

  errno = 0;
  ASSERT_EQ(mkdir(path_with_separators.c_str(), kUserRwx), 0)
      << "mkdir with trailing separators failed: " << strerror(errno);

  // Verify the directory was created correctly, without the separators.
  struct stat statbuf;
  ASSERT_EQ(stat(dir_base.c_str(), &statbuf), 0);
  EXPECT_TRUE(S_ISDIR(statbuf.st_mode));
}

TEST_F(PosixMkdirTest, FailsIfPathExistsAsFile) {
  std::string file_path = test_dir_ + "/a_file";
  int fd = open(file_path.c_str(), O_CREAT | O_WRONLY, 0600);
  ASSERT_NE(fd, -1);
  close(fd);

  EXPECT_EQ(mkdir(file_path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST_F(PosixMkdirTest, FailsIfPathExistsAsDirectory) {
  std::string dir_path = test_dir_ + "/existing_dir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0);

  EXPECT_EQ(mkdir(dir_path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST_F(PosixMkdirTest, FailsIfPathIsSymbolicLink) {
  std::string target_path = test_dir_ + "/target";
  std::string link_path = test_dir_ + "/the_link";
  ASSERT_EQ(mkdir(target_path.c_str(), kUserRwx), 0);
  ASSERT_EQ(symlink(target_path.c_str(), link_path.c_str()), 0);

  // mkdir() on a path that is a symlink should fail with EEXIST.
  EXPECT_EQ(mkdir(link_path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST_F(PosixMkdirTest, FailsIfParentDoesNotExist) {
  std::string path = test_dir_ + "/non_existent_parent/new_dir";
  EXPECT_EQ(mkdir(path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixMkdirTest, FailsIfPathComponentIsNotDirectory) {
  std::string file_path = test_dir_ + "/a_file";
  int fd = open(file_path.c_str(), O_CREAT | O_WRONLY, 0600);
  ASSERT_NE(fd, -1);
  close(fd);

  std::string bad_path = file_path + "/new_dir";
  errno = 0;
  EXPECT_EQ(mkdir(bad_path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST_F(PosixMkdirTest, FailsWithSymbolicLinkLoop) {
  std::string link_a_path = test_dir_ + "/link_a";
  std::string link_b_path = test_dir_ + "/link_b";

  ASSERT_EQ(symlink("link_b", link_a_path.c_str()), 0);
  ASSERT_EQ(symlink("link_a", link_b_path.c_str()), 0);

  std::string path_in_loop = test_dir_ + "/link_a/new_dir";
  errno = 0;
  EXPECT_EQ(mkdir(path_in_loop.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, ELOOP);
}

TEST_F(PosixMkdirTest, FailsOnPathTooLong) {
  std::string long_name(kSbFileMaxPath + 1, 'b');
  std::string long_path = test_dir_ + "/" + long_name;
  errno = 0;
  EXPECT_EQ(mkdir(long_path.c_str(), kUserRwx), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

}  // namespace
}  // namespace nplb
