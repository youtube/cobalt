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

#include <unistd.h>

#include <sys/stat.h>

#include <string>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

bool FileExists(const char* path) {
  struct stat info;
  return stat(path, &info) == 0;
}

bool DirectoryExists(const char* path) {
  struct stat info;
  return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

TEST(PosixFileDeleteTest, SunnyDayDeleteExistingFile) {
  ScopedRandomFile file;

  EXPECT_TRUE(FileExists(file.filename().c_str()));
  EXPECT_TRUE(unlink(file.filename().c_str()) == 0);
}

TEST(PosixFileDeleteTest, SunnyDayDeleteExistingDirectory) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  const std::string& path = file.filename();

  EXPECT_FALSE(FileExists(path.c_str()));
  EXPECT_TRUE(mkdir(path.c_str(), 0700) == 0 || DirectoryExists(path.c_str()));
  EXPECT_TRUE(DirectoryExists(path.c_str()));
  EXPECT_TRUE(rmdir(path.c_str()) == 0);
}

TEST(PosixFileDeleteTest, RainyDayNonExistentDirErrors) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  EXPECT_FALSE(FileExists(file.filename().c_str()));
  EXPECT_TRUE(rmdir(file.filename().c_str()));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
