// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <cerrno>
#include <cstring>
#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFstatatTest, SuccessAbsolutePathEmptyFlags) {
  ScopedRandomFile random_file;
  struct stat sb;
  memset(&sb, 0, sizeof(sb));

  // If path is absolute, dirfd is ignored. We pass an invalid fd to prove it.
  int result = fstatat(-1, random_file.filename().c_str(), &sb, 0);
  EXPECT_EQ(result, 0);
  if (result == 0) {
    EXPECT_TRUE(S_ISREG(sb.st_mode));
    EXPECT_EQ(sb.st_size, random_file.size());
  }
}

TEST(PosixFstatatTest, SuccessRelativeDirfd) {
  std::string temp_dir_path = GetTempDir();
  ASSERT_FALSE(temp_dir_path.empty());

  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0) << "Failed to open temp dir: " << strerror(errno);

  ScopedRandomFile random_file;
  std::string filename =
      random_file.filename().substr(temp_dir_path.length() + 1);

  struct stat sb;
  memset(&sb, 0, sizeof(sb));
  int result = fstatat(dirfd, filename.c_str(), &sb, 0);
  EXPECT_EQ(result, 0) << "fstatat failed: " << strerror(errno);
  if (result == 0) {
    EXPECT_TRUE(S_ISREG(sb.st_mode));
    EXPECT_EQ(sb.st_size, random_file.size());
  }

  close(dirfd);
}

TEST(PosixFstatatTest, SuccessSymlinkNoFollow) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  ScopedRandomFile random_file;
  std::string target_filename =
      random_file.filename().substr(GetTempDir().length() + 1);
  std::string link_filename = target_filename + ".link";
  std::string link_path = temp_dir.path() + kSbFileSepString + link_filename;

  ASSERT_EQ(symlink(random_file.filename().c_str(), link_path.c_str()), 0)
      << "Failed to create symlink: " << strerror(errno);

  struct stat sb;

  int dirfd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0) << "Failed to open temp dir: " << strerror(errno);

  // 1. With AT_SYMLINK_NOFOLLOW, it should stat the link itself.
  memset(&sb, 0, sizeof(sb));
  EXPECT_EQ(fstatat(dirfd, link_filename.c_str(), &sb, AT_SYMLINK_NOFOLLOW), 0)
      << "fstatat failed: " << strerror(errno);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));

  // 2. Without AT_SYMLINK_NOFOLLOW, it should stat the target.
  memset(&sb, 0, sizeof(sb));
  EXPECT_EQ(fstatat(dirfd, link_filename.c_str(), &sb, 0), 0)
      << "fstatat failed: " << strerror(errno);
  EXPECT_TRUE(S_ISREG(sb.st_mode));
  EXPECT_EQ(sb.st_size, random_file.size());

  close(dirfd);
}

TEST(PosixFstatatTest, SuccessEmptyPath) {
  // AT_EMPTY_PATH is a Linux extension, so it may not exist on all platforms.
  // In the case it doesn't exist, we skip this test, and the specified behavior
  // of `fstatat` with an empty path is covered by the ErrorNoentEmptyPath test.
#ifndef AT_EMPTY_PATH
  GTEST_SKIP() << "AT_EMPTY_PATH not defined";
#endif

  std::string temp_dir_path = GetTempDir();
  ASSERT_FALSE(temp_dir_path.empty());

  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0) << "Failed to open temp dir: " << strerror(errno);

  struct stat sb_dirfd;
  ASSERT_EQ(fstat(dirfd, &sb_dirfd), 0);

  struct stat sb_fstatat;
  memset(&sb_fstatat, 0, sizeof(sb_fstatat));

  int result = fstatat(dirfd, "", &sb_fstatat, AT_EMPTY_PATH);
  EXPECT_EQ(result, 0) << "fstatat failed: " << strerror(errno);
  if (result == 0) {
    EXPECT_EQ(sb_fstatat.st_ino, sb_dirfd.st_ino);
    EXPECT_EQ(sb_fstatat.st_dev, sb_dirfd.st_dev);
    EXPECT_TRUE(S_ISDIR(sb_fstatat.st_mode));
  }

  close(dirfd);
}

TEST(PosixFstatatTest, ErrorBadf) {
  struct stat sb;
  errno = 0;
  // Use a relative path and an invalid fd.
  EXPECT_EQ(fstatat(-1, "relative_path", &sb, 0), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFstatatTest, ErrorInvalidFlags) {
  ScopedRandomFile random_file;
  struct stat sb;
  errno = 0;
  EXPECT_EQ(fstatat(AT_FDCWD, random_file.filename().c_str(), &sb, 0xFFFF), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixFstatatTest, ErrorNotDir) {
  ScopedRandomFile random_file;
  int file_fd = open(random_file.filename().c_str(), O_RDONLY);
  ASSERT_GE(file_fd, 0);

  struct stat sb;
  errno = 0;
  // `dirfd` is a regular file (not a directory), so resolving a relative path
  // should fail with ENOTDIR.
  EXPECT_EQ(fstatat(file_fd, "relative_path", &sb, 0), -1);
  EXPECT_EQ(errno, ENOTDIR);

  close(file_fd);
}

TEST(PosixFstatatTest, ErrorNoentEmptyPath) {
  std::string temp_dir_path = GetTempDir();
  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  struct stat sb;
  errno = 0;
  // Path is empty, but AT_EMPTY_PATH is NOT set.
  EXPECT_EQ(fstatat(dirfd, "", &sb, 0), -1);
  EXPECT_EQ(errno, ENOENT);

  close(dirfd);
}

TEST(PosixFstatatTest, ErrorNoentNonExistent) {
  std::string temp_dir_path = GetTempDir();
  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  struct stat sb;
  errno = 0;
  EXPECT_EQ(fstatat(dirfd, "does_not_exist", &sb, 0), -1);
  EXPECT_EQ(errno, ENOENT);

  close(dirfd);
}

TEST(PosixFstatatTest, ErrorNotDirComponent) {
  std::string temp_dir_path = GetTempDir();
  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  ScopedRandomFile random_file;
  std::string filename =
      random_file.filename().substr(temp_dir_path.length() + 1);

  struct stat sb;
  errno = 0;
  // filename is a file, so filename + "/sub" has a non-directory component.
  std::string bad_path = filename + "/sub";
  EXPECT_EQ(fstatat(dirfd, bad_path.c_str(), &sb, 0), -1);
  EXPECT_EQ(errno, ENOTDIR);

  close(dirfd);
}

TEST(PosixFstatatTest, ErrorNameTooLong) {
  std::string temp_dir_path = GetTempDir();
  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  std::string long_name(kSbFileMaxPath + 1, 'a');
  struct stat sb;
  errno = 0;
  EXPECT_EQ(fstatat(dirfd, long_name.c_str(), &sb, 0), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);

  close(dirfd);
}

TEST(PosixFstatatTest, ErrorLoop) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());
  const std::string& temp_dir_path = temp_dir.path();

  std::string link_a = "link_a";
  std::string link_b = "link_b";
  std::string link_a_path = temp_dir_path + "/" + link_a;
  std::string link_b_path = temp_dir_path + "/" + link_b;

  ASSERT_EQ(symlink(link_b.c_str(), link_a_path.c_str()), 0);
  ASSERT_EQ(symlink(link_a.c_str(), link_b_path.c_str()), 0);

  int dirfd = open(temp_dir_path.c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  struct stat sb;
  errno = 0;
  EXPECT_EQ(fstatat(dirfd, link_a.c_str(), &sb, 0), -1);
  EXPECT_EQ(errno, ELOOP);

  close(dirfd);
}

// The name of this test is intentionally "Acces" to match the expected errno
// value EACCES.
TEST(PosixFstatatTest, ErrorAcces) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string protected_dir = temp_dir.path() + "/protected";
  ASSERT_EQ(mkdir(protected_dir.c_str(), 0700), 0);

  std::string file_in_protected = protected_dir + "/inner_file";
  int fd = open(file_in_protected.c_str(), O_CREAT | O_WRONLY, 0600);
  ASSERT_NE(fd, -1);
  close(fd);

  // Open the parent directory of the protected directory
  int dirfd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dirfd, 0);

  // Remove search (execute) permission from the protected directory.
  ASSERT_EQ(chmod(protected_dir.c_str(), 0600), 0);

  struct stat sb;
  errno = 0;
  // Try to stat "protected/inner_file" relative to dirfd
  EXPECT_EQ(fstatat(dirfd, "protected/inner_file", &sb, 0), -1);
  EXPECT_EQ(errno, EACCES);

  // Restore permissions for cleanup
  chmod(protected_dir.c_str(), 0700);
  close(dirfd);
}

}  // namespace
}  // namespace nplb
