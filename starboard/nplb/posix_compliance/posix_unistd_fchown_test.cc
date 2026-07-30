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

TEST(PosixFchownTest, SunnyDay) {
  if (geteuid() != 0) {
    GTEST_SKIP() << "Test must be run as root to change ownership.";
  }

  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  char temp_file_path[kSbFileMaxPath];
  int result = snprintf(temp_file_path, kSbFileMaxPath, "%s/fchown_test.tmp",
                        cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  FILE* fp = fopen(temp_file_path, "w");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";
  int fd = fileno(fp);
  ASSERT_NE(fd, -1);

  struct stat stat_buf;
  ASSERT_EQ(fstat(fd, &stat_buf), 0);

  // Change owner and group to nobody/nogroup (usually 65534)
  ASSERT_EQ(fchown(fd, 65534, 65534), 0);
  ASSERT_EQ(fstat(fd, &stat_buf), 0);
  EXPECT_EQ(static_cast<int>(stat_buf.st_uid), 65534);
  EXPECT_EQ(static_cast<int>(stat_buf.st_gid), 65534);

  // Change back to root
  ASSERT_EQ(fchown(fd, 0, 0), 0);
  ASSERT_EQ(fstat(fd, &stat_buf), 0);
  EXPECT_EQ(static_cast<int>(stat_buf.st_uid), 0);
  EXPECT_EQ(static_cast<int>(stat_buf.st_gid), 0);

  fclose(fp);
  remove(temp_file_path);
}

TEST(PosixFchownTest, RainyDayInvalidFd) {
  errno = 0;
  int result = fchown(-1, 0, 0);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFchownTest, RainyDayPermissionDenied) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "This test must be run as a non-root user.";
  }

  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  char temp_file_path[kSbFileMaxPath];
  int result = snprintf(temp_file_path, kSbFileMaxPath, "%s/fchown_test.tmp",
                        cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  FILE* fp = fopen(temp_file_path, "w");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";
  int fd = fileno(fp);
  ASSERT_NE(fd, -1);

  // Try to change ownership to root. This should fail for non-root users.
  errno = 0;
  result = fchown(fd, 0, 0);
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EPERM);

  fclose(fp);
  remove(temp_file_path);
}

}  // namespace
}  // namespace nplb
