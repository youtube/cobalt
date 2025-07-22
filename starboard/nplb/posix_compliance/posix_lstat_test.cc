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

  - ENAMETOOLONG: Testing for pathnames that exceed the maximum length is not
    easily portable, as the `PATH_MAX` limit can vary significantly across
    different systems.

  - ENOMEM: Reliably simulating an out-of-memory condition is not feasible
    in a standard unit test environment.

  - EFAULT: Testing with an invalid `buf` pointer that is outside the
    process's address space is not reliably portable.
*/

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/file.h"
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
      rmdir(path_);   // Works for empty directories.
    }
  }
  const char* c_str() const { return path_; }

 private:
  const char* path_;
};

constexpr unsigned long ONE = 1;
constexpr unsigned long TWO = 2;
constexpr unsigned long THREE = 3;

TEST(PosixLstatTest, LstatOnExistingFile) {
  const int kFileSize = 12;
  starboard::nplb::ScopedRandomFile random_file(kFileSize);

  struct stat sb;
  EXPECT_EQ(lstat(random_file.filename().c_str(), &sb), 0);
  EXPECT_TRUE(S_ISREG(sb.st_mode));
  EXPECT_EQ(sb.st_size, kFileSize);
  EXPECT_EQ(sb.st_nlink, ONE);
}

TEST(PosixLstatTest, LstatOnExistingDirectory) {
  const char* dir_path = "test_dir.tmp";
  TempPath temp_dir(dir_path);
  ASSERT_EQ(mkdir(dir_path, 0755), 0);

  struct stat sb;
  EXPECT_EQ(lstat(dir_path, &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));
  // A directory should have at least 2 links: one for its own entry
  // and one for the "." entry within it.
  EXPECT_GE(sb.st_nlink, TWO);
}

TEST(PosixLstatTest, DirectoryWithSubdirectory) {
  const char* parent_dir_path = "parent_dir.tmp";
  std::string child_dir_path_str = std::string(parent_dir_path) + "/child";

  // The TempPath destructors will clean up in reverse order of declaration,
  // removing the child directory before the parent.
  TempPath temp_child(child_dir_path_str.c_str());
  TempPath temp_parent(parent_dir_path);

  ASSERT_EQ(mkdir(parent_dir_path, 0755), 0);
  ASSERT_EQ(mkdir(child_dir_path_str.c_str(), 0755), 0);

  struct stat sb;
  EXPECT_EQ(lstat(parent_dir_path, &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));
  // A directory with one subdirectory has 3 links:
  // 1. Its own entry in the parent directory.
  // 2. The "." entry within itself.
  // 3. The ".." entry within the subdirectory pointing back to it.
  EXPECT_EQ(sb.st_nlink, THREE);
}

TEST(PosixLstatTest, LstatOnSymbolicLinkToFile) {
  starboard::nplb::ScopedRandomFile target_file;

  const char* link_path = "link_to_file.tmp";
  TempPath temp_link(link_path);
  std::string target_filename = target_file.filename();

  ASSERT_EQ(symlink(target_filename.c_str(), link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  // lstat should report the type of the link itself, not the target.
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  // The size of a symlink is the length of the path it contains.
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), target_filename.length());
  EXPECT_EQ(sb.st_nlink, ONE);
}

TEST(PosixLstatTest, LstatOnSymbolicLinkToDirectory) {
  const char* dir_path = "target_dir.tmp";
  const char* link_path = "link_to_dir.tmp";
  TempPath temp_dir(dir_path);
  TempPath temp_link(link_path);

  ASSERT_EQ(mkdir(dir_path, 0755), 0);
  ASSERT_EQ(symlink(dir_path, link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), strlen(dir_path));
  EXPECT_EQ(sb.st_nlink, ONE);
}

TEST(PosixLstatTest, LstatOnDanglingSymbolicLink) {
  const char* target_path = "non_existent_target";
  const char* link_path = "dangling_link.tmp";
  TempPath temp_link(link_path);

  // Create a symlink to a target that does not exist.
  ASSERT_EQ(symlink(target_path, link_path), 0);

  struct stat sb;
  EXPECT_EQ(lstat(link_path, &sb), 0);
  EXPECT_TRUE(S_ISLNK(sb.st_mode));
  EXPECT_GE(sb.st_size, 0);
  EXPECT_EQ(static_cast<unsigned long>(sb.st_size), strlen(target_path));
  EXPECT_EQ(sb.st_nlink, ONE);
}

TEST(LstatTest, PathComponentNotADirectory) {
  starboard::nplb::ScopedRandomFile file_as_dir;
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
}  // namespace starboard
