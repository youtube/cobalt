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

#include "starboard/loader_app/app_key_files.h"

#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace loader_app {
namespace {

const char kTestAppKey[] = "test_app_key";
const char kTestAppKeyDir[] = "test_app_key_dir";

class AppKeyFilesTest : public testing::Test {
 protected:
  virtual void SetUp() {
    std::vector<char> temp_path(kSbFileMaxPath, 0);
    ASSERT_TRUE(SbSystemGetPath(kSbSystemPathTempDirectory, temp_path.data(),
                                temp_path.size()));
    dir_ = temp_path.data();
    dir_ += kSbFileSepString;
    dir_ += kTestAppKeyDir;
    SbDirectoryCreate(dir_.c_str());
  }

  std::string dir_;
};

TEST_F(AppKeyFilesTest, TestGoodKeyFile) {
  std::string file_path = GetGoodAppKeyFilePath(dir_, kTestAppKey);
  ASSERT_FALSE(file_path.empty());
  if (SbFileExists(file_path.c_str())) {
    SbFileDelete(file_path.c_str());
  }
  ASSERT_FALSE(SbFileExists(file_path.c_str()));
  ASSERT_TRUE(CreateAppKeyFile(file_path));
  ASSERT_TRUE(SbFileExists(file_path.c_str()));
  SbFileDelete(file_path.c_str());
}

TEST_F(AppKeyFilesTest, TestBadKeyFile) {
  std::string file_path = GetBadAppKeyFilePath(dir_, kTestAppKey);
  ASSERT_FALSE(file_path.empty());
  if (SbFileExists(file_path.c_str())) {
    SbFileDelete(file_path.c_str());
  }
  ASSERT_FALSE(SbFileExists(file_path.c_str()));
  ASSERT_TRUE(CreateAppKeyFile(file_path));
  ASSERT_TRUE(SbFileExists(file_path.c_str()));
  SbFileDelete(file_path.c_str());
}

TEST_F(AppKeyFilesTest, TestGoodKeyFileInvalidInput) {
  std::string file_path = GetGoodAppKeyFilePath("", kTestAppKey);
  ASSERT_TRUE(file_path.empty());

  file_path = GetGoodAppKeyFilePath(dir_, "");
  ASSERT_TRUE(file_path.empty());
}

TEST_F(AppKeyFilesTest, TestBadKeyFileInvalidInput) {
  std::string file_path = GetBadAppKeyFilePath("", kTestAppKey);
  ASSERT_TRUE(file_path.empty());

  file_path = GetBadAppKeyFilePath(dir_, "");
  ASSERT_TRUE(file_path.empty());
}

TEST_F(AppKeyFilesTest, TestAnyGoodKeyFile) {
  ASSERT_FALSE(AnyGoodAppKeyFile(dir_));
  std::string file_path = GetGoodAppKeyFilePath(dir_, kTestAppKey);
  ASSERT_FALSE(file_path.empty());
  ASSERT_TRUE(CreateAppKeyFile(file_path));
  ASSERT_TRUE(SbFileExists(file_path.c_str()));
  ASSERT_TRUE(AnyGoodAppKeyFile(dir_));
  SbFileDelete(file_path.c_str());
}

}  // namespace
}  // namespace loader_app
}  // namespace starboard
