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

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileDeleteTest, SunnyDayDeleteExistingFile) {
  ScopedRandomFile file;

  EXPECT_TRUE(SbFileExists(file.filename().c_str()));
  EXPECT_TRUE(SbFileDelete(file.filename().c_str()));
}

TEST(SbFileDeleteTest, SunnyDayDeleteExistingDirectory) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  const std::string& path = file.filename();

  EXPECT_FALSE(SbFileExists(path.c_str()));
  EXPECT_TRUE(SbDirectoryCreate(path.c_str()));
  EXPECT_TRUE(SbDirectoryCanOpen(path.c_str()));
  EXPECT_TRUE(SbFileDelete(path.c_str()));
}

TEST(SbFileDeleteTest, RainyDayNonExistentFileErrors) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);

  EXPECT_FALSE(SbFileExists(file.filename().c_str()));
  EXPECT_TRUE(SbFileDelete(file.filename().c_str()));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
