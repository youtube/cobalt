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

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFdopendirTest, SunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
  EXPECT_NE(fd, -1);

  DIR* directory = fdopendir(fd);
  EXPECT_TRUE(directory != NULL);
  EXPECT_TRUE(closedir(directory) == 0);
  // closedir should also close the fd.
}

TEST(PosixFdopendirTest, SunnyDayStaticContent) {
  for (auto dir_path : GetFileTestsDirectoryPaths()) {
    int fd = open(dir_path.c_str(), O_RDONLY | O_DIRECTORY);
    EXPECT_NE(fd, -1) << dir_path;

    DIR* directory = fdopendir(fd);
    EXPECT_TRUE(directory != NULL) << dir_path;
    EXPECT_TRUE(closedir(directory) == 0);
  }
}

TEST(PosixFdopendirTest, ManySunnyDay) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Filename is " << path.c_str();

  const int kMany = kSbFileMaxOpen / 2;  // Avoid hitting fd limits.
  std::vector<DIR*> directories(kMany, 0);
  std::vector<int> fds(kMany, -1);

  for (size_t i = 0; i < directories.size(); ++i) {
    fds[i] = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    EXPECT_NE(fds[i], -1);
    directories[i] = fdopendir(fds[i]);
    EXPECT_TRUE(directories[i] != NULL);
  }

  for (size_t i = 0; i < directories.size(); ++i) {
    EXPECT_TRUE(closedir(directories[i]) == 0);
  }
}

TEST(PosixFdopendirTest, FailsInvalidFileDescriptor) {
  DIR* directory = fdopendir(-1);
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFdopendirTest, FailsOnFileDescriptorOfRegularFile) {
  ScopedRandomFile file;
  int fd = open(file.filename().c_str(), O_RDONLY);
  EXPECT_NE(fd, -1);

  DIR* directory = fdopendir(fd);
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, ENOTDIR);

  close(fd);
}

TEST(PosixFdopendirTest, FailsOnClosedFileDescriptor) {
  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());

  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
  EXPECT_NE(fd, -1);
  EXPECT_EQ(close(fd), 0);

  DIR* directory = fdopendir(fd);
  EXPECT_EQ(directory, nullptr);
  EXPECT_EQ(errno, EBADF);
}

}  // namespace
}  // namespace nplb
