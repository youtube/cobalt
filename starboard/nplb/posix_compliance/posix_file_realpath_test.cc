// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

// If PATH_MAX is not defined from limits.h, define it.
#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif  // !defined(PATH_MAX)

namespace starboard::nplb {
namespace {

class PosixFileRealpathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary directory for our test files.
    snprintf(temp_dir_, sizeof(temp_dir_), "/tmp/realpath_test_XXXXXX");
    char* result = mkdtemp(temp_dir_);
    ASSERT_NE(nullptr, result)
        << "Failed to create temp directory: " << strerror(errno);

    // Create a file within the temporary directory.
    snprintf(file_path_, sizeof(file_path_), "%s/test_file.txt", temp_dir_);
    FILE* file = fopen(file_path_, "w");
    ASSERT_NE(nullptr, file)
        << "Failed to create test file: " << strerror(errno);
    fclose(file);
  }

  void TearDown() override {
    unlink(file_path_);
    rmdir(temp_dir_);
  }

  char temp_dir_[PATH_MAX];
  char file_path_[PATH_MAX];
};

TEST_F(PosixFileRealpathTest, SunnyDay) {
  char resolved_path[PATH_MAX];
  ASSERT_NE(nullptr, realpath(file_path_, resolved_path))
      << "realpath failed: " << strerror(errno);
  EXPECT_STREQ(resolved_path, file_path_);
}

TEST_F(PosixFileRealpathTest, NonExistentFile) {
  char non_existent_file[PATH_MAX];
  snprintf(non_existent_file, sizeof(non_existent_file), "%s/no_file_here",
           temp_dir_);
  char resolved_path[PATH_MAX];
  EXPECT_EQ(nullptr, realpath(non_existent_file, resolved_path));
  EXPECT_EQ(ENOENT, errno);
}

TEST_F(PosixFileRealpathTest, RelativePath) {
  char current_dir[PATH_MAX];
  ASSERT_NE(nullptr, getcwd(current_dir, sizeof(current_dir)));
  ASSERT_EQ(0, chdir(temp_dir_));

  char relative_path[] = "../realpath_test_XXXXXX/test_file.txt";
  // The directory name is randomized, so we need to construct it.
  char* dir_name = strrchr(temp_dir_, '/');
  snprintf(relative_path, sizeof(relative_path), "../%s/test_file.txt",
           dir_name + 1);
  char resolved_path[PATH_MAX];
  ASSERT_NE(nullptr, realpath("test_file.txt", resolved_path))
      << "realpath with relative path failed: " << strerror(errno);
  EXPECT_STREQ(resolved_path, file_path_);

  // Change back to original directory.
  ASSERT_EQ(0, chdir(current_dir));
}

TEST_F(PosixFileRealpathTest, SymbolicLink) {
  char symlink_path[PATH_MAX];
  snprintf(symlink_path, sizeof(symlink_path), "%s/test_link", temp_dir_);
  int ret = symlink(file_path_, symlink_path);
  ASSERT_EQ(0, ret) << "symlink failed: " << strerror(errno);

  char resolved_path[PATH_MAX];
  char* result = realpath(symlink_path, resolved_path);

  ASSERT_NE(nullptr, result)
      << "realpath with symlink failed: " << strerror(errno);
  EXPECT_STREQ(resolved_path, file_path_);

  unlink(symlink_path);
}

TEST_F(PosixFileRealpathTest, NullResolvedPath) {
  // This test checks the POSIX.1-2008/GNU extension where realpath allocates
  // the buffer if the second argument is NULL. This behavior is not
  // guaranteed to be supported on all platforms, so if this call returns
  // null with a EINVAL error, we can assume that the POSIX version is
  // pre 2008.
  errno = 0;
  char* result = realpath(file_path_, NULL);

  if (result == nullptr) {
    if (errno == EINVAL) {
      // If we fail with this specific errno, we can assume that the platform
      // does not support the extension, so we just pass the test.
      SUCCEED() << "realpath(path, NULL) is not supported on this platform "
                << "(returned NULL with errno=EINVAL), which is acceptable.";
      return;
    }
  }
  ASSERT_NE(nullptr, result)
      << "realpath with NULL resolved_path failed: " << strerror(errno);
  EXPECT_STREQ(result, file_path_);
  free(result);  // realpath with NULL allocates memory that we must free.
}

}  // namespace
}  // namespace starboard::nplb
