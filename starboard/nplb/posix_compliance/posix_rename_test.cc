// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixRenameTest, SucceedsForValidPath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string old_path = temp_dir.path() + kSbFileSepString + "temp_old.txt";
  std::string new_path = temp_dir.path() + kSbFileSepString + "temp_new.txt";

  FILE* file = fopen(old_path.c_str(), "w");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(fclose(file), 0);

  errno = 0;
  int result = rename(old_path.c_str(), new_path.c_str());

  EXPECT_EQ(result, 0) << "rename failed with error: " << strerror(errno);

  EXPECT_EQ(access(old_path.c_str(), F_OK), -1);
  EXPECT_EQ(access(new_path.c_str(), F_OK), 0);
}

TEST(PosixRenameTest, FailsForInvalidPath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string invalid_path =
      temp_dir.path() + kSbFileSepString + "non_existent_file.txt";
  std::string new_path = temp_dir.path() + kSbFileSepString + "new_file.txt";

  errno = 0;
  int result = rename(invalid_path.c_str(), new_path.c_str());

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixRenameTest, SucceedsForDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string old_dir_path =
      temp_dir.path() + kSbFileSepString + "temp_old_dir";
  std::string new_dir_path =
      temp_dir.path() + kSbFileSepString + "temp_new_dir";

  ASSERT_EQ(mkdir(old_dir_path.c_str(), 0755), 0);

  std::string file_path = old_dir_path + kSbFileSepString + "temp_file.txt";

  FILE* file = fopen(file_path.c_str(), "w");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(fclose(file), 0);

  errno = 0;
  int result = rename(old_dir_path.c_str(), new_dir_path.c_str());

  EXPECT_EQ(result, 0) << "rename failed with error: " << strerror(errno);

  EXPECT_EQ(access(old_dir_path.c_str(), F_OK), -1);
  EXPECT_EQ(access(new_dir_path.c_str(), F_OK), 0);

  std::string new_file_path = new_dir_path + kSbFileSepString + "temp_file.txt";
  EXPECT_EQ(access(new_file_path.c_str(), F_OK), 0);
}

}  // namespace
}  // namespace nplb
