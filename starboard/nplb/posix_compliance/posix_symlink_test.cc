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

  - EACCES: Triggering a permission-denied error for writing to the directory
    containing the new path is difficult to set up in a portable way.

  - ENAMETOOLONG: Testing for pathnames that exceed the maximum length is not
    easily portable, as the `PATH_MAX` limit can vary significantly.

  - EIO: Triggering a low-level I/O error is not something that can be
    reliably simulated in a unit test.

  - ELOOP: Triggering a "too many symbolic links" error during path resolution
    of the newpath is complex to orchestrate.

  - ENOSPC: Reliably simulating a "no space left on device" error is not
    feasible in a unit test environment.

  - EROFS: Testing on a read-only filesystem requires specific test setup that
    is not always available or portable.
*/

#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <string>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// A helper class to manage temporary paths for testing.
// It automatically cleans up created paths upon destruction.
class TempPath {
 public:
  explicit TempPath(const char* path) : path_(path) {}
  ~TempPath() {
    if (path_) {
      unlink(path_);  // Works for files and symbolic links.
    }
  }
  const char* c_str() const { return path_; }

 private:
  const char* path_;
};

TEST(PosixSymlinkTest, SuccessfulCreation) {
  starboard::nplb::ScopedRandomFile target_file;
  const char* link_path = "success_link.tmp";
  TempPath temp_link(link_path);

  // Create the symbolic link.
  EXPECT_EQ(symlink(target_file.filename().c_str(), link_path), 0);

  // Verify that the link was created and points to the correct target.
  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));

  char read_buf[1024];
  ssize_t len = readlink(link_path, read_buf, sizeof(read_buf) - 1);
  ASSERT_GT(len, 0);
  read_buf[len] = '\0';
  EXPECT_STREQ(target_file.filename().c_str(), read_buf);
}

TEST(PosixSymlinkTest, FailsIfNewPathExists) {
  starboard::nplb::ScopedRandomFile existing_file;
  const char* target_path = "some_target";

  // Attempt to create a symlink where the newpath already exists.
  EXPECT_EQ(symlink(target_path, existing_file.filename().c_str()), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST(PosixSymlinkTest, CanCreateDanglingLink) {
  const char* non_existent_target = "this_file_does_not_exist";
  const char* link_path = "dangling_link.tmp";
  TempPath temp_link(link_path);

  // It is valid to create a symbolic link to a target that does not exist.
  EXPECT_EQ(symlink(non_existent_target, link_path), 0);

  // Verify that the dangling link was created.
  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
}

TEST(PosixSymlinkTest, FailsWithEmptyOldPath) {
  const char* link_path = "empty_oldpath_link.tmp";
  TempPath temp_link(link_path);

  // The target of a symlink cannot be an empty string.
  EXPECT_EQ(symlink("", link_path), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixSymlinkTest, FailsWithEmptyNewPath) {
  const char* target_path = "some_target";

  // The path for the new symlink cannot be an empty string.
  EXPECT_EQ(symlink(target_path, ""), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixSymlinkTest, FailsWithNullOldPath) {
  const char* link_path = "null_oldpath_link.tmp";
  TempPath temp_link(link_path);

  // A NULL oldpath should result in a "Bad address" error.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
  EXPECT_EQ(symlink(NULL, link_path), -1);
#pragma clang diagnostic pop
  EXPECT_EQ(errno, EFAULT);
}

TEST(PosixSymlinkTest, FailsWithNullNewPath) {
  const char* target_path = "some_target";

  // A NULL newpath should result in a "Bad address" error.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
  EXPECT_EQ(symlink(target_path, NULL), -1);
#pragma clang diagnostic pop
  EXPECT_EQ(errno, EFAULT);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
