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

#include <string>

#include "starboard/directory.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbDirectoryCanOpenTest, SunnyDay) {
  std::string path = starboard::nplb::GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
}

TEST(SbDirectoryCanOpenTest, SunnyDayStaticContent) {
  for (auto dir_path : GetFileTestsDirectoryPaths()) {
    EXPECT_TRUE(SbDirectoryCanOpen(dir_path.c_str()))
        << "Can't open: " << dir_path;
  }
}

TEST(SbDirectoryCanOpenTest, FailureMissingStaticContent) {
  std::string directory_path = GetFileTestsDataDir();
  std::string missing_dir = directory_path + kSbFileSepChar + "missing_dir";
  EXPECT_FALSE(SbDirectoryCanOpen(missing_dir.c_str()));
}

TEST(SbDirectoryCanOpenTest, FailureNull) {
  EXPECT_FALSE(SbDirectoryCanOpen(NULL));
}

TEST(SbDirectoryCanOpenTest, FailureEmpty) {
  EXPECT_FALSE(SbDirectoryCanOpen(""));
}

TEST(SbDirectoryCanOpenTest, FailureRegularFile) {
  starboard::nplb::ScopedRandomFile file;

  EXPECT_TRUE(SbFileExists(file.filename().c_str()));
  EXPECT_FALSE(SbDirectoryCanOpen(file.filename().c_str()));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
