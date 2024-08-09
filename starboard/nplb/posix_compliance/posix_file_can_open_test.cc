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

#include <fcntl.h>

#include <string>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileCanOpenTest, NonExistingFileFails) {
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  bool result = starboard::FileCanOpen(filename.c_str(), O_RDONLY);
  EXPECT_FALSE(result);

  result =
      starboard::FileCanOpen(filename.c_str(), O_CREAT | O_EXCL | O_WRONLY);
  EXPECT_FALSE(result);

  result = starboard::FileCanOpen(filename.c_str(), O_CREAT | O_RDWR);
  EXPECT_FALSE(result);
}

TEST(PosixFileCanOpenTest, ExistingFileSucceeds) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  bool result = starboard::FileCanOpen(filename.c_str(), O_RDONLY);
  EXPECT_TRUE(result);

  result =
      starboard::FileCanOpen(filename.c_str(), O_CREAT | O_EXCL | O_WRONLY);
  EXPECT_TRUE(result);

  result = starboard::FileCanOpen(filename.c_str(), O_CREAT | O_RDWR);
  EXPECT_TRUE(result);
}

TEST(PosixFileCanOpenTest, NonExistingStaticContentFileFails) {
  std::string directory_path = GetFileTestsDataDir();
  std::string missing_file = directory_path + kSbFileSepChar + "missing_file";
  EXPECT_FALSE(starboard::FileCanOpen(missing_file.c_str(), O_RDONLY));
}

TEST(PosixFileCanOpenTest, ExistingStaticContentFileSucceeds) {
  for (auto path : GetFileTestsFilePaths()) {
    EXPECT_TRUE(starboard::FileCanOpen(path.c_str(), O_RDONLY))
        << "Can't open: " << path;
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
