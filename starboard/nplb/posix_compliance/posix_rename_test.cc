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

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixRenameTest, SucceedsForValidPath) {
  char temp_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathTempDirectory, temp_path, kSbFileMaxPath));

  char old_path[kSbFileMaxPath];
  snprintf(old_path, kSbFileMaxPath, "%s%ctemp_old.txt", temp_path,
           kSbFileSepChar);

  char new_path[kSbFileMaxPath];
  snprintf(new_path, kSbFileMaxPath, "%s%ctemp_new.txt", temp_path,
           kSbFileSepChar);

  FILE* file = fopen(old_path, "w");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(fclose(file), 0);

  errno = 0;
  int result = rename(old_path, new_path);

  EXPECT_EQ(result, 0) << "rename failed with error: " << strerror(errno);

  EXPECT_EQ(access(old_path, F_OK), -1);
  EXPECT_EQ(access(new_path, F_OK), 0);

  EXPECT_EQ(unlink(new_path), 0);
}

TEST(PosixRenameTest, FailsForInvalidPath) {
  const char* invalid_path = "non_existent_file.txt";
  const char* new_path = "new_file.txt";

  errno = 0;
  int result = rename(invalid_path, new_path);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixRenameTest, SucceedsForDirectory) {
  char temp_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathTempDirectory, temp_path, kSbFileMaxPath));

  char old_dir_path[kSbFileMaxPath];
  snprintf(old_dir_path, kSbFileMaxPath, "%s%ctemp_old_dir", temp_path,
           kSbFileSepChar);

  char new_dir_path[kSbFileMaxPath];
  snprintf(new_dir_path, kSbFileMaxPath, "%s%ctemp_new_dir", temp_path,
           kSbFileSepChar);

  ASSERT_EQ(mkdir(old_dir_path, 0755), 0);

  char file_path[kSbFileMaxPath];
  snprintf(file_path, kSbFileMaxPath, "%s%ctemp_file.txt", old_dir_path,
           kSbFileSepChar);

  FILE* file = fopen(file_path, "w");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(fclose(file), 0);

  errno = 0;
  int result = rename(old_dir_path, new_dir_path);

  EXPECT_EQ(result, 0) << "rename failed with error: " << strerror(errno);

  EXPECT_EQ(access(old_dir_path, F_OK), -1);
  EXPECT_EQ(access(new_dir_path, F_OK), 0);

  char new_file_path[kSbFileMaxPath];
  snprintf(new_file_path, kSbFileMaxPath, "%s%ctemp_file.txt", new_dir_path,
           kSbFileSepChar);
  EXPECT_EQ(access(new_file_path, F_OK), 0);

  EXPECT_EQ(unlink(new_file_path), 0);
  EXPECT_EQ(rmdir(new_dir_path), 0);
}

}  // namespace
}  // namespace nplb
