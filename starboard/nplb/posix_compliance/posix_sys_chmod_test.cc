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
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixChmodTest, SunnyDay) {
  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  char temp_file_path[kSbFileMaxPath];
  int result =
      snprintf(temp_file_path, kSbFileMaxPath, "%s/chmod_test.tmp", cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  FILE* fp = fopen(temp_file_path, "w");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";
  fclose(fp);

  struct stat stat_buf;
  ASSERT_EQ(stat(temp_file_path, &stat_buf), 0);

  // Set read-only for owner
  ASSERT_EQ(chmod(temp_file_path, S_IRUSR), 0);
  ASSERT_EQ(stat(temp_file_path, &stat_buf), 0);
  EXPECT_EQ(static_cast<int>(stat_buf.st_mode & S_IRWXU), S_IRUSR);

  // Set read-write for owner
  ASSERT_EQ(chmod(temp_file_path, S_IRUSR | S_IWUSR), 0);
  ASSERT_EQ(stat(temp_file_path, &stat_buf), 0);
  EXPECT_EQ(static_cast<int>(stat_buf.st_mode & S_IRWXU), S_IRUSR | S_IWUSR);

  remove(temp_file_path);
}

TEST(PosixChmodTest, RainyDayInvalidPath) {
  const char* invalid_path = "/this/path/does/not/exist/12345";
  errno = 0;
  int result = chmod(invalid_path, S_IRUSR);

  // On failure, chmod should return -1 and set errno.
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixChmodTest, SunnyDayReadOnly) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "Test is not meaningful when run as root.";
  }
  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  char temp_file_path[kSbFileMaxPath];
  int result =
      snprintf(temp_file_path, kSbFileMaxPath, "%s/chmod_test.tmp", cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  FILE* fp = fopen(temp_file_path, "w");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";
  fclose(fp);

  // Set read-only for owner
  ASSERT_EQ(chmod(temp_file_path, S_IRUSR), 0);

  // Attempt to open for writing, which should fail.
  FILE* fp_write = fopen(temp_file_path, "w");
  EXPECT_EQ(nullptr, fp_write);

  remove(temp_file_path);
}

TEST(PosixChmodTest, SunnyDayCtime) {
  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  char temp_file_path[kSbFileMaxPath];
  int result =
      snprintf(temp_file_path, kSbFileMaxPath, "%s/chmod_test.tmp", cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  FILE* fp = fopen(temp_file_path, "w");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";
  fclose(fp);

  struct stat stat_buf1;
  ASSERT_EQ(stat(temp_file_path, &stat_buf1), 0);

  // Wait for at least one second to ensure a different ctime.
  sleep(2);

  ASSERT_EQ(chmod(temp_file_path, S_IRUSR | S_IWUSR), 0);

  struct stat stat_buf2;
  ASSERT_EQ(stat(temp_file_path, &stat_buf2), 0);

  EXPECT_GT(stat_buf2.st_ctime, stat_buf1.st_ctime);

  remove(temp_file_path);
}

}  // namespace
}  // namespace nplb
