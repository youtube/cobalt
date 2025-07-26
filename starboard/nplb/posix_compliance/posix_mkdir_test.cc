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
  - ENAMETOOLONG (path length limits)
  - EIO (I/O errors)
  - ELOOP (symbolic link loops)
  - ENOSPC (out-of-space/inode errors)
  - EROFS (read-only filesystem errors)
*/

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixMkdirTest, SuccessfulCreation) {
  const char* dir_path = "success_dir.tmp";
  const mode_t mode = 0755;

  // Create the directory.
  ASSERT_EQ(mkdir(dir_path, mode), 0);

  // Verify that the directory was created with the correct properties.
  struct stat sb;
  ASSERT_EQ(stat(dir_path, &sb), 0);
  EXPECT_TRUE(S_ISDIR(sb.st_mode));
  // The requested mode is masked by the process's umask. We can only check
  // that the resulting permissions are a subset of the requested ones.
  EXPECT_EQ((sb.st_mode & ~S_IFMT) & mode, (sb.st_mode & ~S_IFMT));
  rmdir(dir_path);
}

TEST(PosixMkdirTest, FailsIfPathExistsAsFile) {
  starboard::nplb::ScopedRandomFile existing_file;

  // Attempt to create a directory where a file with the same name exists.
  EXPECT_EQ(mkdir(existing_file.filename().c_str(), 0755), -1);
  EXPECT_EQ(errno, EEXIST);
}

TEST(PosixMkdirTest, FailsIfPathExistsAsDirectory) {
  const char* dir_path = "existing_dir.tmp";
  ASSERT_EQ(mkdir(dir_path, 0755), 0);

  // Attempt to create a directory that already exists.
  EXPECT_EQ(mkdir(dir_path, 0755), -1);
  EXPECT_EQ(errno, EEXIST);
  rmdir(dir_path);
}

TEST(PosixMkdirTest, FailsIfParentDoesNotExist) {
  const char* path = "non_existent_parent/new_dir";

  // Attempt to create a directory where a component of the path does not exist.
  EXPECT_EQ(mkdir(path, 0755), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixMkdirTest, FailsWithEmptyPath) {
  // An empty path should result in a "No such file or directory" error.
  EXPECT_EQ(mkdir("", 0755), -1);
  EXPECT_EQ(errno, ENOENT);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
