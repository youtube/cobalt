// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
#include <unistd.h>

#include <string>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFileDeleteTest, SunnyDayDeleteExistingFile) {
  ScopedRandomFile file;

  const std::string& filename = file.filename();

  EXPECT_TRUE(FileExists(filename.c_str()))
      << "File should exist before deletion: " << filename;
  const int unlink_result = unlink(filename.c_str());
  EXPECT_EQ(unlink_result, 0) << "unlink failed for file: " << filename
                              << " with error: " << strerror(errno);
  EXPECT_FALSE(FileExists(filename.c_str()))
      << "File should not exist after deletion: " << filename;
}

TEST(PosixFileDeleteTest, SunnyDayDeleteExistingDirectory) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  const std::string& path = file.filename();

  EXPECT_FALSE(FileExists(path.c_str()))
      << "File should not exist before directory creation: " << path;
  const int mkdir_result = mkdir(path.c_str(), 0700);
  EXPECT_EQ(mkdir_result, 0) << "mkdir failed for directory: " << path
                             << " with error: " << strerror(errno);
  EXPECT_TRUE(DirectoryExists(path.c_str()))
      << "Directory should exist after creation: " << path;
  const int rmdir_result = rmdir(path.c_str());
  EXPECT_EQ(rmdir_result, 0) << "rmdir failed for directory: " << path
                             << " with error: " << strerror(errno);
  EXPECT_FALSE(DirectoryExists(path.c_str()))
      << "Directory should not exist after deletion: " << path;
}

TEST(PosixFileDeleteTest, RainyDayNonExistentDirErrors) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);
  const std::string& path = file.filename();

  EXPECT_FALSE(FileExists(path.c_str()))
      << "File should not exist before rmdir: " << path;
  const int rmdir_result = rmdir(path.c_str());
  EXPECT_NE(rmdir_result, 0)
      << "rmdir should have failed for non-existent directory: " << path;
}

}  // namespace
}  // namespace nplb
