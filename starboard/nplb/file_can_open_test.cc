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

TEST(SbFileCanOpenTest, NonExistingFileFails) {
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  bool result = SbFileCanOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead);
  EXPECT_FALSE(result);

  result = SbFileCanOpen(filename.c_str(), kSbFileCreateOnly | kSbFileWrite);
  EXPECT_FALSE(result);

  result = SbFileCanOpen(filename.c_str(),
                         kSbFileOpenAlways | kSbFileRead | kSbFileWrite);
  EXPECT_FALSE(result);
}

TEST(SbFileCanOpenTest, ExistingFileSucceeds) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  bool result = SbFileCanOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead);
  EXPECT_TRUE(result);

  result = SbFileCanOpen(filename.c_str(), kSbFileCreateOnly | kSbFileWrite);
  EXPECT_TRUE(result);

  result = SbFileCanOpen(filename.c_str(),
                         kSbFileOpenAlways | kSbFileRead | kSbFileWrite);
  EXPECT_TRUE(result);
}

TEST(SbFileCanOpenTest, NonExistingStaticContentFileFails) {
  std::string directory_path = GetFileTestsDataDir();
  std::string missing_file = directory_path + kSbFileSepChar + "missing_file";
  EXPECT_FALSE(
      SbFileCanOpen(missing_file.c_str(), kSbFileOpenOnly | kSbFileRead));
}

TEST(SbFileCanOpenTest, ExistingStaticContentFileSucceeds) {
  for (auto path : GetFileTestsFilePaths()) {
    EXPECT_TRUE(SbFileCanOpen(path.c_str(), kSbFileOpenOnly | kSbFileRead))
        << "Can't open: " << path;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
