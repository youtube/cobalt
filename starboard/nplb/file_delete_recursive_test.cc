// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include <sys/stat.h>

#include <cstddef>
#include <string>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {
using ::starboard::SbFileDeleteRecursive;

const size_t kDirectoryCount = 3;
const size_t kFileCount = 3;

const char kRoot[] = {"SbFileDeleteRecursiveTest"};

const char* kDirectories[kDirectoryCount] = {
    "",
    "test1",
    "test2",
};

const char* kFiles[kFileCount] = {
    "file1",
    "test1/file2",
    "test2/file3",
};

TEST(SbFileDeleteRecursiveTest, SunnyDayDeleteExistingPath) {
  std::string path;
  const std::string& tmp = GetTempDir();

  // Create the directory tree.
  for (size_t i = 0; i < kDirectoryCount; ++i) {
    path = tmp + kSbFileSepString + kRoot + kSbFileSepString + kDirectories[i];

    EXPECT_FALSE(FileExists(path.c_str()));
    EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 ||
                DirectoryExists(path.c_str()));
    EXPECT_TRUE(DirectoryExists(path.c_str()));
  }

  int file = -1;

  // Create files in our directory tree.
  for (size_t i = 0; i < kFileCount; ++i) {
    path = tmp + kSbFileSepString + kRoot + kSbFileSepString + kFiles[i];

    EXPECT_FALSE(FileExists(path.c_str()));

    file = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);

    EXPECT_TRUE(file >= 0);
    EXPECT_EQ(close(file), 0);
    EXPECT_TRUE(FileExists(path.c_str()));
  }

  path = tmp + kSbFileSepString + kRoot;

  EXPECT_TRUE(SbFileDeleteRecursive(path.c_str(), false));
  EXPECT_FALSE(FileExists(path.c_str()));
}

TEST(SbFileDeleteRecursiveTest, SunnyDayDeletePreserveRoot) {
  const std::string root = GetTempDir() + kSbFileSepString + kRoot;

  EXPECT_FALSE(FileExists(root.c_str()));
  EXPECT_TRUE(mkdir(root.c_str(), 0700) == 0 || DirectoryExists(root.c_str()));
  EXPECT_TRUE(DirectoryExists(root.c_str()));

  EXPECT_TRUE(SbFileDeleteRecursive(root.c_str(), true));
  EXPECT_TRUE(FileExists(root.c_str()));
  EXPECT_TRUE(SbFileDeleteRecursive(root.c_str(), false));
  EXPECT_FALSE(FileExists(root.c_str()));
}

TEST(SbFileDeleteRecursiveTest, RainyDayDeleteFileIgnoresPreserveRoot) {
  const std::string& path = GetTempDir() + kSbFileSepString + "file1";

  EXPECT_FALSE(FileExists(path.c_str()));

  int file = -1;

  file = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);

  EXPECT_TRUE(file >= 0);
  EXPECT_EQ(close(file), 0);
  EXPECT_TRUE(FileExists(path.c_str()));
  EXPECT_TRUE(SbFileDeleteRecursive(path.c_str(), true));
  EXPECT_FALSE(FileExists(path.c_str()));
}

TEST(SbFileDeleteRecursiveTest, RainyDayNonExistentPathErrors) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  EXPECT_FALSE(FileExists(file.filename().c_str()));
  EXPECT_FALSE(SbFileDeleteRecursive(file.filename().c_str(), false));
}

}  // namespace
}  // namespace nplb
