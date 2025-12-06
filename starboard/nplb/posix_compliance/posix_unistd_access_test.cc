// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixAccessTest : public ::testing::Test {
 protected:
  void SetUp() override {
    char cache_path[kSbFileMaxPath];
    ASSERT_TRUE(SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path,
                                kSbFileMaxPath));
    test_dir_ = std::string(cache_path) + kSbFileSepString + "access_test";
    mkdir(test_dir_.c_str(), 0777);

    file_path_ = test_dir_ + kSbFileSepString + "test_file.txt";
    FILE* fp = fopen(file_path_.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "test");
    fclose(fp);

    readonly_file_path_ = test_dir_ + kSbFileSepString + "readonly.txt";
    fp = fopen(readonly_file_path_.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "test");
    fclose(fp);
    chmod(readonly_file_path_.c_str(), S_IRUSR | S_IRGRP | S_IROTH);
  }

  void TearDown() override {
    remove(file_path_.c_str());
    remove(readonly_file_path_.c_str());
    rmdir(test_dir_.c_str());
  }

  std::string test_dir_;
  std::string file_path_;
  std::string readonly_file_path_;
};

TEST_F(PosixAccessTest, SunnyDayFileExists) {
  EXPECT_EQ(access(file_path_.c_str(), F_OK), 0);
}

TEST_F(PosixAccessTest, SunnyDayFileRead) {
  EXPECT_EQ(access(file_path_.c_str(), R_OK), 0);
}

TEST_F(PosixAccessTest, SunnyDayFileWrite) {
  EXPECT_EQ(access(file_path_.c_str(), W_OK), 0);
}

TEST_F(PosixAccessTest, SunnyDayFileReadWrite) {
  EXPECT_EQ(access(file_path_.c_str(), R_OK | W_OK), 0);
}

TEST_F(PosixAccessTest, RainyDayFileDoesNotExist) {
  std::string non_existent_file = test_dir_ + kSbFileSepString + "non_existent";
  errno = 0;
  EXPECT_EQ(access(non_existent_file.c_str(), F_OK), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixAccessTest, RainyDayFileNoWrite) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test is not meaningful when run as root.";
  }
  errno = 0;
  EXPECT_EQ(access(readonly_file_path_.c_str(), W_OK), -1);
  EXPECT_EQ(errno, EACCES);
}

TEST_F(PosixAccessTest, RainyDayFileNoExecute) {
  errno = 0;
  EXPECT_EQ(access(file_path_.c_str(), X_OK), -1);
  EXPECT_EQ(errno, EACCES);
}

TEST_F(PosixAccessTest, SunnyDayDirectoryExists) {
  EXPECT_EQ(access(test_dir_.c_str(), F_OK), 0);
}

}  // namespace
}  // namespace nplb
