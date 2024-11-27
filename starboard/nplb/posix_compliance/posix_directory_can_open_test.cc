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

#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixDirectoryCanOpenTest, SunnyDay) {
  std::string path = starboard::nplb::GetTempDir();
  struct stat file_info;
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(stat(path.c_str(), &file_info) == 0);

  EXPECT_TRUE(S_ISDIR(file_info.st_mode));
}

TEST(PosixDirectoryCanOpenTest, SunnyDayStaticContent) {
  for (auto dir_path : GetFileTestsDirectoryPaths()) {
    struct stat file_info;
    stat(dir_path.c_str(), &file_info);
    EXPECT_TRUE(S_ISDIR(file_info.st_mode)) << "Can't open: " << dir_path;
  }
}

TEST(PosixDirectoryCanOpenTest, FLAKY_FailureMissingStaticContent) {
  std::string directory_path = GetFileTestsDataDir();
  std::string missing_dir = directory_path + kSbFileSepChar + "missing_dir";
  struct stat file_info;
  stat(missing_dir.c_str(), &file_info);
  EXPECT_FALSE(S_ISDIR(file_info.st_mode));
}

TEST(PosixDirectoryCanOpenTest, FailureEmpty) {
  struct stat file_info;
  EXPECT_FALSE(stat("", &file_info) == 0);
}

TEST(PosixDirectoryCanOpenTest, FailureRegularFile) {
  starboard::nplb::ScopedRandomFile file;
  struct stat file_info;

  EXPECT_TRUE(stat(file.filename().c_str(), &file_info) == 0);
  EXPECT_FALSE(S_ISDIR(file_info.st_mode));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
