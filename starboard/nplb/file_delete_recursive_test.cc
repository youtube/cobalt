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

#include <string>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const size_t kDirectoryCount = 3;
const size_t kFileCount = 3;

const char kRoot[] = {"SbFileDeleteRecursiveTest"};

const char* kDirectories[kDirectoryCount] = {
    "", "test1", "test2",
};

const char* kFiles[kFileCount] = {
    "file1", "test1/file2", "test2/file3",
};

TEST(SbFileDeleteRecursiveTest, SunnyDayDeleteExistingPath) {
  std::string path;
  const std::string& tmp = GetTempDir();

  // Create the directory tree.
  for (size_t i = 0; i < kDirectoryCount; ++i) {
    path = tmp + kSbFileSepString + kRoot + kSbFileSepString + kDirectories[i];

    EXPECT_FALSE(SbFileExists(path.c_str()));
    EXPECT_TRUE(SbDirectoryCreate(path.c_str()));
    EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
  }

  SbFileError err = kSbFileOk;
  SbFile file = kSbFileInvalid;

  // Create files in our directory tree.
  for (size_t i = 0; i < kFileCount; ++i) {
    path = tmp + kSbFileSepString + kRoot + kSbFileSepString + kFiles[i];

    EXPECT_FALSE(SbFileExists(path.c_str()));

    file = SbFileOpen(path.c_str(), kSbFileCreateAlways | kSbFileWrite, NULL,
                      &err);

    EXPECT_EQ(kSbFileOk, err);
    EXPECT_TRUE(SbFileClose(file));
    EXPECT_TRUE(SbFileExists(path.c_str()));
  }

  path = tmp + kSbFileSepString + kRoot;

  EXPECT_TRUE(SbFileDeleteRecursive(path.c_str(), false));
  EXPECT_FALSE(SbFileExists(path.c_str()));
}

TEST(SbFileDeleteRecursiveTest, SunnyDayDeletePreserveRoot) {
  const std::string root = GetTempDir() + kSbFileSepString + kRoot;

  EXPECT_FALSE(SbFileExists(root.c_str()));
  EXPECT_TRUE(SbDirectoryCreate(root.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(root.c_str()));

  EXPECT_TRUE(SbFileDeleteRecursive(root.c_str(), true));
  EXPECT_TRUE(SbFileExists(root.c_str()));
  EXPECT_TRUE(SbFileDeleteRecursive(root.c_str(), false));
  EXPECT_FALSE(SbFileExists(root.c_str()));
}

TEST(SbFileDeleteRecursiveTest, RainyDayDeleteFileIgnoresPreserveRoot) {
  const std::string& path = GetTempDir() + kSbFileSepString + "file1";

  EXPECT_FALSE(SbFileExists(path.c_str()));

  SbFileError err = kSbFileOk;
  SbFile file = kSbFileInvalid;

  file = SbFileOpen(path.c_str(), kSbFileCreateAlways | kSbFileWrite, NULL,
                    &err);

  EXPECT_EQ(kSbFileOk, err);
  EXPECT_TRUE(SbFileClose(file));
  EXPECT_TRUE(SbFileExists(path.c_str()));
  EXPECT_TRUE(SbFileDeleteRecursive(path.c_str(), true));
  EXPECT_FALSE(SbFileExists(path.c_str()));
}

TEST(SbFileDeleteRecursiveTest, RainyDayNonExistentPathErrors) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  EXPECT_FALSE(SbFileExists(file.filename().c_str()));
  EXPECT_FALSE(SbFileDeleteRecursive(file.filename().c_str(), false));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
