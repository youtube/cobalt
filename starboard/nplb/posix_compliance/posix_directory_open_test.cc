// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
  Untestable Scenarios for opendir:

  - [ENFILE]: Cannot reliably trigger the system-wide file descriptor limit.
  - [EMFILE]: Triggering the per-process file descriptor limit can lead to
    flaky tests.
*/

#include <dirent.h>
#include <sys/stat.h>

#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixDirectoryOpenTest, SunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  DIR* directory = opendir(path.c_str());
  EXPECT_TRUE(directory != NULL);
  EXPECT_TRUE(closedir(directory) == 0);
}

TEST(PosixDirectoryOpenTest, SunnyDayStaticContent) {
  for (auto dir_path : GetFileTestsDirectoryPaths()) {
    DIR* directory = opendir(dir_path.c_str());
    EXPECT_TRUE(directory != NULL) << dir_path;
    EXPECT_TRUE(closedir(directory) == 0);
  }
}

TEST(PosixDirectoryOpenTest, SunnyDayWithNullError) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  DIR* directory = opendir(path.c_str());
  EXPECT_TRUE(directory != NULL);
  EXPECT_TRUE(closedir(directory) == 0);
}

TEST(PosixDirectoryOpenTest, ManySunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  const int kMany = kSbFileMaxOpen;
  std::vector<DIR*> directories(kMany, 0);

  for (size_t i = 0; i < directories.size(); ++i) {
    directories[i] = opendir(path.c_str());
    EXPECT_TRUE(directories[i] != NULL);
  }

  for (size_t i = 0; i < directories.size(); ++i) {
    EXPECT_TRUE(closedir(directories[i]) == 0);
  }
}

TEST(PosixDirectoryOpenTest, FailsInvalidPath) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  // Funny way to make sure the directory seems valid but doesn't exist.
  int len = static_cast<int>(path.length());
  if (path[len - 1] != 'z') {
    path[len - 1] = 'z';
  } else {
    path[len - 1] = 'y';
  }

  struct stat info;
  ASSERT_FALSE(stat(path.c_str(), &info) == 0);

  DIR* directory = opendir(path.c_str());
  EXPECT_FALSE(directory != NULL);
  if (directory != NULL) {
    closedir(directory);
  }
}

TEST(PosixDirectoryOpenTest, FailsEmptyPath) {
  DIR* directory = opendir("");
  EXPECT_FALSE(directory != NULL);
  if (directory != NULL) {
    closedir(directory);
  }
}

TEST(PosixDirectoryOpenTest, FailsRegularFile) {
  ScopedRandomFile file;

  DIR* directory = opendir(file.filename().c_str());
  EXPECT_FALSE(directory != NULL);
  if (directory != NULL) {
    closedir(directory);
  }
}

TEST(PosixDirectoryOpenTest, FailsNoAccess) {
  // This test will not work correctly if run as root, as root bypasses
  // file permission checks.
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test cannot run as root; permission checks are bypassed.";
  }

  std::string dir_path = GetTempDir() + kSbFileSepString + "no_access_dir";
  ASSERT_EQ(mkdir(dir_path.c_str(), kUserRwx), 0);

  // Remove read and search permissions.
  ASSERT_EQ(chmod(dir_path.c_str(), S_IWUSR), 0);

  errno = 0;
  DIR* directory = opendir(dir_path.c_str());
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, EACCES);

  // Cleanup: Restore permissions to allow removal.
  chmod(dir_path.c_str(), kUserRwx);
  rmdir(dir_path.c_str());
}

TEST(PosixDirectoryOpenTest, FailsSymlinkLoop) {
  std::string temp_dir = GetTempDir();
  std::string path_a = temp_dir + kSbFileSepString + "loop_a";
  std::string path_b = temp_dir + kSbFileSepString + "loop_b";

  // Clean up any old links that might exist from a previous failed run.
  unlink(path_a.c_str());
  unlink(path_b.c_str());

  // Create a circular dependency: a -> b and b -> a. This is a guaranteed
  // symbolic link loop that the system must detect.
  ASSERT_EQ(symlink(path_b.c_str(), path_a.c_str()), 0)
      << "Failed to create symlink a->b: " << strerror(errno);
  ASSERT_EQ(symlink(path_a.c_str(), path_b.c_str()), 0)
      << "Failed to create symlink b->a: " << strerror(errno);

  errno = 0;
  DIR* directory = opendir(path_a.c_str());
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, ELOOP);

  // Cleanup
  unlink(path_a.c_str());
  unlink(path_b.c_str());
}

TEST(PosixDirectoryOpenTest, FailsNameTooLong) {
  std::string temp_dir = GetTempDir();
  std::string long_name(kSbFileMaxPath + 1, 'a');
  std::string long_path = temp_dir + kSbFileSepString + long_name;

  errno = 0;
  DIR* directory = opendir(long_path.c_str());
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, ENAMETOOLONG);
}

}  // namespace
}  // namespace nplb
