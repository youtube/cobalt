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

static void CheckFileCanOpen(const std::string& filename,
                             int flags,
                             bool expected_result) {
  const bool result = starboard::FileCanOpen(filename.c_str(), flags);
  EXPECT_EQ(result, expected_result)
      << "FileCanOpen(" << filename << ", flags=" << flags << ") "
      << "expected " << (expected_result ? "success" : "failure")
      << ", but got " << (result ? "success" : "failure") << ".";
}

TEST(PosixFileCanOpenTest, NonExistingFileFails) {
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  CheckFileCanOpen(filename, O_RDONLY, false);
  CheckFileCanOpen(filename, O_CREAT | O_EXCL | O_WRONLY, false);
  CheckFileCanOpen(filename, O_CREAT | O_RDWR, false);
}

TEST(PosixFileCanOpenTest, ExistingFileSucceeds) {
  ScopedRandomFile random_file;
  const std::string& filename = random_file.filename();

  CheckFileCanOpen(filename, O_RDONLY, true);
  CheckFileCanOpen(filename, O_CREAT | O_EXCL | O_WRONLY, true);
  CheckFileCanOpen(filename, O_CREAT | O_RDWR, true);
}

TEST(PosixFileCanOpenTest, NonExistingStaticContentFileFails) {
  std::string directory_path = GetFileTestsDataDir();
  std::string missing_file = directory_path + kSbFileSepChar + "missing_file";
  CheckFileCanOpen(missing_file, O_RDONLY, false);
}

TEST(PosixFileCanOpenTest, ExistingStaticContentFileSucceeds) {
  for (auto path : GetFileTestsFilePaths()) {
    CheckFileCanOpen(path, O_RDONLY, true);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
