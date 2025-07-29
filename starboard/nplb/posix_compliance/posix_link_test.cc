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

  - EMLINK: Triggering an error for too many links to a file is not practical
    as it would require creating thousands of links.

  - ENAMETOOLONG: Testing for pathnames that exceed the maximum length is not
    easily portable, as the `PATH_MAX` limit can vary significantly.

  - ENOSPC / EROFS / EXDEV: Reliably simulating filesystem-specific errors like
    "no space", "read-only", or "cross-device link" is not feasible in a
    standard unit test environment.
*/

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixLinkTest, SuccessfulCreation) {
  ScopedRandomFile old_path;
  const std::string new_path = old_path.filename() + ".link";

  ASSERT_EQ(link(old_path.filename().c_str(), new_path.c_str()), 0);

  struct stat old_sb, new_sb;
  EXPECT_EQ(lstat(old_path.filename().c_str(), &old_sb), 0);
  EXPECT_EQ(lstat(new_path.c_str(), &new_sb), 0);

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
  ScopedRandomFile old_path;
  ScopedRandomFile new_path;  // This file already exists

  EXPECT_EQ(link(old_path.filename().c_str(), new_path.filename().c_str()), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST(PosixLinkTest, FailsOnDirectory) {
  const char* dir_path = "dir_to_link.tmp";
  const char* new_path = "new_dir_link.tmp";
  ASSERT_EQ(mkdir(dir_path, 0755), 0);

  EXPECT_EQ(link(dir_path, new_path), -1);
  EXPECT_EQ(errno, EPERM);

  rmdir(dir_path);
}

TEST(PosixLinkTest, FailsWithSymbolicLinkLoop) {
  // Setup a temporary directory for this test.
  const char* dir_path = "eloop_test_dir";
  ASSERT_EQ(mkdir(dir_path, 0755), 0);

  ScopedRandomFile old_path;
  const std::string link_a_path = std::string(dir_path) + "/link_a";
  const std::string link_b_path = std::string(dir_path) + "/link_b";

  // Create a symlink loop using relative paths: link_a -> link_b, and link_b ->
  // link_a
  ASSERT_EQ(symlink("link_b", link_a_path.c_str()), 0);
  ASSERT_EQ(symlink("link_a", link_b_path.c_str()), 0);

  // Attempt to create a link where the new path contains the loop.
  const std::string new_path_with_loop = link_a_path + "/new_link";
  EXPECT_EQ(link(old_path.filename().c_str(), new_path_with_loop.c_str()), -1);
  EXPECT_EQ(errno, ELOOP);

  // Cleanup
  unlink(link_a_path.c_str());
  unlink(link_b_path.c_str());
  rmdir(dir_path);
}

TEST(PosixLinkTest, FailsIfPathComponentNotDirectory) {
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

}  // namespace
}  // namespace nplb
}  // namespace starboard
