// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/reset_evergreen_update.h"

#include <sys/stat.h>

#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace loader_app {
namespace {

bool FileExists(const char* path) {
#if SB_API_VERSION < 16
  return SbFileExists(path);
#else
  struct stat info;
  return stat(path, &info) == 0;
#endif
}

TEST(ResetEvergreenUpdateTest, TestSunnyDayFile) {
  std::vector<char> storage_path(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathStorageDirectory,
                              storage_path.data(), kSbFileMaxPath));

  std::string file_path = storage_path.data();
  file_path += kSbFileSepString;
  file_path += "test.txt";
  SB_LOG(INFO) << "file: " << file_path;

  {
    SbFileError error;
    ScopedFile file(file_path.c_str(), kSbFileOpenAlways | kSbFileWrite,
                    nullptr, &error);
    ASSERT_EQ(kSbFileOk, error) << "Failed to open file for writing";
    std::string data = "abc";
    int bytes_written = file.WriteAll(data.c_str(), data.size());
  }

  ASSERT_TRUE(FileExists(file_path.data()));

  ASSERT_TRUE(ResetEvergreenUpdate());

  ASSERT_FALSE(FileExists(file_path.data()));
  ASSERT_TRUE(FileExists(storage_path.data()));
}

TEST(ResetEvergreenUpdateTest, TestSunnyDaySubdir) {
  std::vector<char> storage_path(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathStorageDirectory,
                              storage_path.data(), kSbFileMaxPath));

  std::string subdir_path = storage_path.data();
  subdir_path += kSbFileSepString;
  subdir_path += "test";
  ASSERT_TRUE(SbDirectoryCreate(subdir_path.c_str()));

  std::string file_path = subdir_path.data();
  file_path += kSbFileSepString;
  file_path += "test.txt";
  SB_LOG(INFO) << "file: " << file_path;

  {
    SbFileError error;
    ScopedFile file(file_path.c_str(), kSbFileOpenAlways | kSbFileWrite,
                    nullptr, &error);
    ASSERT_EQ(kSbFileOk, error) << "Failed to open file for writing";
    std::string data = "abc";
    int bytes_written = file.WriteAll(data.c_str(), data.size());
  }

  ASSERT_TRUE(FileExists(file_path.data()));

  ASSERT_TRUE(ResetEvergreenUpdate());

  ASSERT_FALSE(FileExists(file_path.data()));
  ASSERT_TRUE(FileExists(storage_path.data()));
}
}  // namespace
}  // namespace loader_app
}  // namespace starboard

#endif  //  SB_IS(EVERGREEN_COMPATIBLE)
