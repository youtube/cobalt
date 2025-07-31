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

#include <dirent.h>
#include <sys/stat.h>
#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void ExpectFileExists(const char* path) {
  struct stat info;
  EXPECT_TRUE(stat(path, &info) == 0) << "Filename is " << path;
}

TEST(PosixDirectoryOpenTest, SunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  ExpectFileExists(path.c_str());

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
  ExpectFileExists(path.c_str());

  DIR* directory = opendir(path.c_str());
  EXPECT_TRUE(directory != NULL);
  EXPECT_TRUE(closedir(directory) == 0);
}

TEST(PosixDirectoryOpenTest, ManySunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  ExpectFileExists(path.c_str());

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
  ExpectFileExists(path.c_str());

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

}  // namespace
}  // namespace nplb
}  // namespace starboard
