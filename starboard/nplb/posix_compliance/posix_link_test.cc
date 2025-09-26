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

  - EILSEQ: The filename is not a portable filename. This is difficult to test
    as it depends on the underlying filesystem's character set support.

  - EMLINK: Triggering an error for too many links to a file is not practical
    as it would require creating thousands of links.

  - ENOSPC / EROFS / EXDEV: Reliably simulating filesystem-specific errors like
    "no space", "read-only", or "cross-device link" is not feasible in a
    standard unit test environment.
*/

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixLinkTest, SuccessfulCreation) {
  ScopedRandomFile old_file;
  const std::string new_path = old_file.filename() + ".link";

  ASSERT_EQ(link(old_file.filename().c_str(), new_path.c_str()), 0)
      << "link failed with error: " << strerror(errno);

  struct stat old_sb, new_sb;
  EXPECT_EQ(lstat(old_file.filename().c_str(), &old_sb), 0)
      << "lstat failed with error: " << strerror(errno);
  EXPECT_EQ(lstat(new_path.c_str(), &new_sb), 0)
      << "lstat failed with error: " << strerror(errno);

  // Hard links should share the same inode number.
  EXPECT_EQ(old_sb.st_ino, new_sb.st_ino);
  // The link count for the inode should now be 2.
  EXPECT_EQ(old_sb.st_nlink, static_cast<unsigned long>(2));
  EXPECT_EQ(new_sb.st_nlink, static_cast<unsigned long>(2));

  unlink(new_path.c_str());
}

TEST(PosixLinkTest, FailsIfOldPathDoesNotExist) {
  const char* non_existent_path = "this_path_does_not_exist";
  const char* new_path = "new_link.tmp";
  EXPECT_EQ(link(non_existent_path, new_path), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixLinkTest, FailsIfNewPathExists) {
  ScopedRandomFile old_file;
  ScopedRandomFile new_file;

  EXPECT_EQ(link(old_file.filename().c_str(), new_file.filename().c_str()), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST(PosixLinkTest, FailsOnDirectory) {
  const char* dir_path = "dir_to_link.tmp";
  const char* new_path = "new_dir_link.tmp";
  ASSERT_EQ(mkdir(dir_path, kUserRwx), 0)
      << "mkdir failed with error: " << strerror(errno);

  EXPECT_EQ(link(dir_path, new_path), -1);
  EXPECT_EQ(errno, EPERM);

  rmdir(dir_path);
}

TEST(PosixLinkTest, FailsWithSymbolicLinkLoopInDestPath) {
  // Setup a temporary directory for this test.
  const char* dir_path = "eloop_test_dir";
  ASSERT_EQ(mkdir(dir_path, kUserRwx), 0)
      << "mkdir failed with error: " << strerror(errno);

  ScopedRandomFile old_file;
  const std::string link_a_path = std::string(dir_path) + "/link_a";
  const std::string link_b_path = std::string(dir_path) + "/link_b";

  // Create a symlink loop using relative paths: link_a -> link_b, and link_b ->
  // link_a
  ASSERT_EQ(symlink("link_b", link_a_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);
  ASSERT_EQ(symlink("link_a", link_b_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);

  // Attempt to create a link where the new path contains the loop.
  const std::string new_path_with_loop = link_a_path + "/new_link";
  EXPECT_EQ(link(old_file.filename().c_str(), new_path_with_loop.c_str()), -1);
  EXPECT_EQ(errno, ELOOP);

  // Cleanup
  unlink(link_a_path.c_str());
  unlink(link_b_path.c_str());
  rmdir(dir_path);
}

TEST(PosixLinkTest, FailsIfNewPathComponentNotDirectory) {
  ScopedRandomFile file;
  std::string new_path = file.filename() + "/new_link";

  EXPECT_EQ(link(file.filename().c_str(), new_path.c_str()), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST(PosixLinkTest, FailsWithEmptyOldPath) {
  const char* new_path = "new_link.tmp";
  EXPECT_EQ(link("", new_path), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixLinkTest, FailsWithEmptyNewPath) {
  ScopedRandomFile old_path;
  EXPECT_EQ(link(old_path.filename().c_str(), ""), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixLinkTest, FailsIfOldPathIsTooLong) {
  std::string long_path(kSbFileMaxPath + 1, 'a');
  const char* new_path = "new_link.tmp";

  EXPECT_EQ(link(long_path.c_str(), new_path), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST(PosixLinkTest, FailsIfNewPathIsTooLong) {
  ScopedRandomFile old_path;
  std::string long_path(kSbFileMaxPath + 1, 'a');

  EXPECT_EQ(link(old_path.filename().c_str(), long_path.c_str()), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

TEST(PosixLinkTest, FailsWithSymbolicLinkLoopInSourcePath) {
  const char* dir_path = "eloop_test_dir_old";
  ASSERT_EQ(mkdir(dir_path, kUserRwx), 0)
      << "mkdir failed with error: " << strerror(errno);

  const std::string link_a_path = std::string(dir_path) + "/link_a";
  const std::string link_b_path = std::string(dir_path) + "/link_b";
  const std::string new_path = std::string(dir_path) + "/new_link";

  ASSERT_EQ(symlink("link_b", link_a_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);
  ASSERT_EQ(symlink("link_a", link_b_path.c_str()), 0)
      << "symlink failed with error: " << strerror(errno);

  // Attempt to create a link FROM a path that contains a loop in a
  // directory component. This forces path resolution to fail.
  const std::string old_path_with_loop = link_a_path + "/some_file";
  EXPECT_EQ(link(old_path_with_loop.c_str(), new_path.c_str()), -1);
  EXPECT_EQ(errno, ELOOP);

  unlink(link_a_path.c_str());
  unlink(link_b_path.c_str());
  rmdir(dir_path);
}

TEST(PosixLinkTest, FailsIfOldPathComponentNotDirectory) {
  ScopedRandomFile file;
  std::string old_path = file.filename() + "/source_file";
  const char* new_path = "new_link.tmp";

  EXPECT_EQ(link(old_path.c_str(), new_path), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

}  // namespace
}  // namespace nplb
