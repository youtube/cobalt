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
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <cmath>

#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixStatvfsTest, SunnyDay) {
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

TEST(PosixStatvfsTest, RainyDayInvalidPath) {
  const char* invalid_path = "/this/path/does/not/exist/12345";

  struct statvfs buf;
  errno = 0;
  int result = statvfs(invalid_path, &buf);

  // On failure, statvfs should return -1 and set errno.
  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixStatvfsTest, UsageChanges) {
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

  // Use a larger file size to make the change in usage more significant
  // than typical filesystem fluctuations.
  constexpr const size_t k1MB = 1024 * 1024;

  // The number of blocks are in units of f_frsize.
  if (buf_before.f_frsize == 0) {
    GTEST_SKIP() << "f_frsize is 0, cannot determine space usage.";
  }
  size_t available_bytes = buf_before.f_bavail * buf_before.f_frsize;
  if (available_bytes < k1MB * 2) {
    GTEST_SKIP() << "Not enough cache space to run UsageChanges test.";
  }

  const size_t write_size = k1MB;
  char* write_buffer = new char[write_size];
  memset(write_buffer, 'a', write_size);

  FILE* fp = fopen(temp_file_path, "wb");
  ASSERT_NE(nullptr, fp) << "Failed to open temp file.";

  size_t written = fwrite(write_buffer, 1, write_size, fp);
  ASSERT_EQ(fflush(fp), 0);
  int fd = fileno(fp);
  ASSERT_NE(fd, -1);
  ASSERT_EQ(fdatasync(fd), 0) << "fdatasync failed: " << strerror(errno);
  fclose(fp);
  delete[] write_buffer;

  ASSERT_EQ(written, write_size);

  struct statvfs buf_after_creation;
  errno = 0;
  ASSERT_EQ(statvfs(cache_path, &buf_after_creation), 0)
      << "statvfs (after creation) failed: " << strerror(errno);

  ASSERT_LT(buf_after_creation.f_bavail, buf_before.f_bavail)
      << "Filesystem did not reflect file creation.";

  EXPECT_EQ(buf_after_creation.f_blocks, buf_before.f_blocks);
  // On some filesystems, f_files is not constant and may increase when new
  // files are created. Only verify it doesn't decrease.
  EXPECT_GE(buf_after_creation.f_files, buf_before.f_files);

  // Calculate the change in blocks after creation.
  const long long blocks_consumed =
      static_cast<long long>(buf_before.f_bavail) -
      static_cast<long long>(buf_after_creation.f_bavail);
  ASSERT_GT(blocks_consumed, 0);

  ASSERT_EQ(remove(temp_file_path), 0);

  int dir_fd = open(cache_path, O_RDONLY);
  ASSERT_NE(dir_fd, -1) << "Failed to open directory: " << strerror(errno);
  ASSERT_EQ(fsync(dir_fd), 0)
      << "fsync on directory failed: " << strerror(errno);
  close(dir_fd);

  // Allow for some slack in the block comparison. The filesystem might not
  // restore the exact number of blocks due to metadata, journaling, or other
  // concurrent operations.
  const long long kBlockTolerance = std::max(10LL, blocks_consumed / 10);

  struct statvfs buf_after_removal;
  errno = 0;
  ASSERT_EQ(statvfs(cache_path, &buf_after_removal), 0)
      << "statvfs (after removal) failed: " << strerror(errno);

  const long long blocks_freed =
      static_cast<long long>(buf_after_removal.f_bavail) -
      static_cast<long long>(buf_after_creation.f_bavail);

  // We expect the number of freed blocks to be close to the number of
  // consumed blocks.
  EXPECT_NEAR(blocks_freed, blocks_consumed, kBlockTolerance)
      << "Filesystem did not reflect file removal correctly. "
      << "Blocks consumed: " << blocks_consumed
      << ", Blocks freed: " << blocks_freed
      << ", Tolerance: " << kBlockTolerance;
}

}  // namespace
}  // namespace nplb
