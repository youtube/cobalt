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
  Test cases that are not feasible to implement in a unit testing environment:

  - EACCES: Triggering a permission-denied error in a portable way is
    difficult, as it often requires changing file permissions in a way that
    might affect the test runner or other system processes.

  - ELOOP: Creating a scenario with too many symbolic links in a path resolution
    is complex and highly dependent on the filesystem and system configuration.

  - ENOMEM: Reliably simulating an out-of-memory condition is not feasible
    in a standard unit test environment.

  - EFAULT: Testing with an invalid `buf` pointer that is outside the
    process's address space is not reliably portable.
*/

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixLstatTest, LstatOnExistingFile) {
  const int kFileSize = 12;
  ScopedRandomFile random_file(kFileSize);

  struct stat sb;
  EXPECT_EQ(lstat(random_file.filename().c_str(), &sb), 0);
  EXPECT_TRUE(S_ISREG(sb.st_mode));
  EXPECT_EQ(sb.st_size, kFileSize);
  EXPECT_EQ(sb.st_nlink, 1u);
}

TEST(PosixLstatTest, LstatOnExistingDirectory) {
  const char* dir_path = "test_dir.tmp";
  ASSERT_EQ(mkdir(dir_path, 0755), 0);

  struct stat sb;
  EXPECT_EQ(lstat(dir_path, &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));
  // Modern filesystems might have just one link.
  EXPECT_GE(sb.st_nlink, 1u);
  rmdir(dir_path);
}

TEST(PosixLstatTest, DirectoryWithSubdirectory) {
  char template_name[] = "/tmp/lstat_test_XXXXXX";
  char* parent_dir_path = mkdtemp(template_name);
  std::string child_dir_path_str = std::string(parent_dir_path) + "/child";

  ASSERT_EQ(mkdir(child_dir_path_str.c_str(), 0755), 0);

  struct stat sb;
  EXPECT_EQ(lstat(parent_dir_path, &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));
  // A directory with one subdirectory has 3 links:
  // 1. Its own entry in the parent directory.
  // 2. The "." entry within itself.
  // 3. The ".." entry within the subdirectory pointing back to it.
  EXPECT_EQ(sb.st_nlink, 3u);
  rmdir(child_dir_path_str.c_str());
  rmdir(parent_dir_path);
}

TEST(PosixLstatTest, LstatOnSymbolicLinkToFile) {
  ScopedRandomFile target_file;

  const char* link_path = "link_to_file.tmp";
  std::string target_filename = target_file.filename();

  ASSERT_EQ(symlink(target_filename.c_str(), link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  // lstat should report the type of the link itself, not the target.
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  // The size of a symlink is the length of the path it contains.
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), target_filename.length());
  EXPECT_EQ(sb.st_nlink, 1u);
  unlink(link_path);
}

TEST(PosixLstatTest, LstatOnSymbolicLinkToDirectory) {
  const char* dir_path = "target_dir.tmp";
  const char* link_path = "link_to_dir.tmp";

  ASSERT_EQ(mkdir(dir_path, 0755), 0);
  ASSERT_EQ(symlink(dir_path, link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), strlen(dir_path));
  EXPECT_EQ(sb.st_nlink, 1u);
  unlink(link_path);
  rmdir(dir_path);
}

TEST(PosixLstatTest, LstatOnDanglingSymbolicLink) {
  const char* target_path = "non_existent_target";
  const char* link_path = "dangling_link.tmp";

  // Create a symlink to a target that does not exist.
  ASSERT_EQ(symlink(target_path, link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), strlen(target_path));
  EXPECT_EQ(sb.st_nlink, 1u);
  unlink(link_path);
}

TEST(LstatTest, PathComponentNotADirectory) {
  ScopedRandomFile file_as_dir;
  std::string path = file_as_dir.filename() + "/some_file";

  struct stat sb;
  EXPECT_EQ(lstat(path.c_str(), &sb), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST(PosixLstatTest, LstatOnNonExistentPath) {
  struct stat sb;
  EXPECT_EQ(lstat("this_path_does_not_exist", &sb), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixLstatTest, LstatOnEmptyPath) {
  // An empty path should result in a "No such file or directory" error.
  struct stat sb;
  EXPECT_EQ(lstat("", &sb), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixLstatTest, LstatFailsIfPathIsTooLong) {
  // Create a string longer than the maximum allowed path length.
  std::string long_path(kSbFileMaxPath + 1, 'a');
  struct stat sb;

  // Attempt to lstat the overly long path.
  EXPECT_EQ(lstat(long_path.c_str(), &sb), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST(PosixLstatTest, LstatOnNullPath) {
  // A NULL path should result in a "Bad address" error.
  struct stat sb;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
  EXPECT_EQ(lstat(nullptr, &sb), -1);
#pragma clang diagnostic pop
  EXPECT_EQ(errno, EFAULT);
}

}  // namespace
}  // namespace nplb
