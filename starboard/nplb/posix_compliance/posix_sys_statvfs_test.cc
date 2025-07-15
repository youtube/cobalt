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

#include <sys/statvfs.h>

#include <errno.h>
#include <string.h>

#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixSysStatvfsTest, SunnyDay) {
  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  struct statvfs buf;
  errno = 0;
  int result = statvfs(cache_path, &buf);

  ASSERT_EQ(result, 0) << "statvfs failed with error: " << strerror(errno);

  EXPECT_GT(buf.f_bsize, static_cast<SbSystemCapabilityId>(0));
  EXPECT_GT(buf.f_frsize, static_cast<SbSystemCapabilityId>(0));
  EXPECT_GT(buf.f_blocks, static_cast<fsblkcnt_t>(0));
  EXPECT_GT(buf.f_files, static_cast<fsfilcnt_t>(0));
}

TEST(PosixSysStatvfsTest, RainyDayInvalidPath) {
  const char* invalid_path = "/this/path/does/not/exist/12345";

  struct statvfs buf;
  errno = 0;
  int result = statvfs(invalid_path, &buf);

  // On failure, statvfs should return -1 and set errno.
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixSysStatvfsTest, UsageChanges) {
  char cache_path[kSbFileMaxPath];
  ASSERT_TRUE(
      SbSystemGetPath(kSbSystemPathCacheDirectory, cache_path, kSbFileMaxPath));

  struct statvfs buf_before;
  errno = 0;
  ASSERT_EQ(statvfs(cache_path, &buf_before), 0)
      << "statvfs (before) failed: " << strerror(errno);

  ASSERT_GT(buf_before.f_bavail, static_cast<fsblkcnt_t>(0));
  ASSERT_GT(buf_before.f_ffree, static_cast<fsfilcnt_t>(0));

  char temp_file_path[kSbFileMaxPath];
  int result = snprintf(temp_file_path, kSbFileMaxPath, "%s/statvfs_temp.tmp",
                        cache_path);
  ASSERT_GT(result, 0);
  ASSERT_LT(result, static_cast<int>(kSbFileMaxPath));

  constexpr const size_t k4kBytes = 4096;
  const size_t write_size =
      buf_before.f_bsize > 0 ? buf_before.f_bsize : k4kBytes;
  char* write_buffer = new char[write_size];
  memset(write_buffer, 'a', write_size);

  FILE* fp = fopen(temp_file_path, "wb");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";

  size_t written = fwrite(write_buffer, 1, write_size, fp);
  fclose(fp);
  delete[] write_buffer;

  ASSERT_EQ(written, write_size);

  struct statvfs buf_after_creation;
  errno = 0;
  ASSERT_EQ(statvfs(cache_path, &buf_after_creation), 0)
      << "statvfs (after creation) failed: " << strerror(errno);

  EXPECT_LT(buf_after_creation.f_ffree, buf_before.f_ffree);
  EXPECT_LT(buf_after_creation.f_bavail, buf_before.f_bavail);

  // The total number of blocks and files should not change.
  EXPECT_EQ(buf_after_creation.f_blocks, buf_before.f_blocks);
  EXPECT_EQ(buf_after_creation.f_files, buf_before.f_files);

  ASSERT_EQ(remove(temp_file_path), 0);

  // After removing the file, the available space should be restored.
  struct statvfs buf_after_removal;
  errno = 0;
  ASSERT_EQ(statvfs(cache_path, &buf_after_removal), 0)
      << "statvfs (after removal) failed: " << strerror(errno);

  EXPECT_EQ(buf_after_removal.f_ffree, buf_before.f_ffree);
  EXPECT_EQ(buf_after_removal.f_bavail, buf_before.f_bavail);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
