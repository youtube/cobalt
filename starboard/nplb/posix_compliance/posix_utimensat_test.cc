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

/*
  Test cases that are not feasible to implement in a unit testing environment:

  - EACCES: Triggering a permission-denied error in a portable way is
    difficult, as it often requires changing file permissions in a way that
    might affect the test runner or other system processes.

  - ELOOP: Creating a scenario with too many symbolic links in a path resolution
    is complex and highly dependent on the filesystem and system configuration.

  - ENAMETOOLONG: Testing for pathnames that exceed the maximum length is not
    easily portable, as the `PATH_MAX` limit can vary significantly across
    different systems.

  - EROFS: Reliably creating and mounting a read-only filesystem for a test
    is not practical in a unit test environment.

  - EFAULT: Testing with an invalid `times` pointer that is outside the
    process's address space is not reliably portable.
*/

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <type_traits>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Helper function to get the modification time of a file.
time_t GetMTime(const char* path) {
  struct stat stat_buf;
  if (stat(path, &stat_buf) != 0) {
    return -1;
  }
  return stat_buf.st_mtime;
}

time_t GetLinkMTime(const char* path) {
  struct stat stat_buf;
  if (lstat(path, &stat_buf) != 0) {
    return -1;
  }
  return stat_buf.st_mtime;
}

TEST(PosixUtimensatTest, SetToCurrentTimeFromOldTimestamp) {
  ScopedRandomFile file;

  timespec old_times[2];
  old_times[0].tv_sec = 123456789;
  old_times[0].tv_nsec = 0;
  old_times[1].tv_sec = 123456789;
  old_times[1].tv_nsec = 0;

  int initial_set_result =
      utimensat(AT_FDCWD, file.filename().c_str(), old_times, 0);
  ASSERT_EQ(0, initial_set_result) << "Failed to set initial old timestamp.";

  time_t mtime_before = GetMTime(file.filename().c_str());
  ASSERT_EQ(123456789, mtime_before)
      << "Initial timestamp was not set correctly.";

  int result = utimensat(AT_FDCWD, file.filename().c_str(), NULL, 0);
  ASSERT_EQ(0, result) << "utimensat failed: " << strerror(errno);

  time_t mtime_after = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_after) << "Failed to get updated mtime.";

  EXPECT_GT(mtime_after, mtime_before)
      << "Timestamp should have been updated to be greater than the old one.";
}

TEST(PosixUtimensatTest, SetToSpecificTime) {
  ScopedRandomFile file;
  timespec times[2];
  times[0].tv_sec = 123456789;
  times[0].tv_nsec = 0;
  times[1].tv_sec = 123456789;
  times[1].tv_nsec = 0;

  int result = utimensat(AT_FDCWD, file.filename().c_str(), times, 0);
  ASSERT_EQ(0, result) << "utimensat failed: " << strerror(errno);

  time_t mtime_after = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_after) << "Failed to get updated mtime.";
  EXPECT_EQ(123456789, mtime_after)
      << "Timestamp was not set to the specific value.";
}

TEST(PosixUtimensatTest, SetAccessTimeWithUtimeNow) {
  ScopedRandomFile file;
  time_t mtime_before = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_before) << "Failed to get initial mtime.";

  timespec times[2];
  times[0].tv_nsec = UTIME_NOW;
  times[1].tv_nsec = UTIME_OMIT;

  int result = utimensat(AT_FDCWD, file.filename().c_str(), times, 0);
  ASSERT_EQ(0, result) << "utimensat failed: " << strerror(errno);

  time_t mtime_after = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_after) << "Failed to get updated mtime.";
  EXPECT_EQ(mtime_before, mtime_after)
      << "Modification time should have been omitted.";
}

TEST(PosixUtimensatTest, OmitBothTimestamps) {
  ScopedRandomFile file;
  time_t mtime_before = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_before) << "Failed to get initial mtime.";

  timespec times[2];
  times[0].tv_nsec = UTIME_OMIT;
  times[1].tv_nsec = UTIME_OMIT;

  int result = utimensat(AT_FDCWD, file.filename().c_str(), times, 0);
  ASSERT_EQ(0, result) << "utimensat failed: " << strerror(errno);

  time_t mtime_after = GetMTime(file.filename().c_str());
  ASSERT_NE(-1, mtime_after) << "Failed to get updated mtime.";
  EXPECT_EQ(mtime_before, mtime_after)
      << "Timestamps should have been unchanged.";
}

TEST(PosixUtimensatTest, SymlinkNoFollow) {
  ScopedRandomFile target_file;
  std::string link_path = target_file.filename() + ".symlink";

  ASSERT_EQ(0, symlink(target_file.filename().c_str(), link_path.c_str()))
      << "Failed to create symlink.";

  time_t original_file_mtime_before = GetMTime(target_file.filename().c_str());

  timespec times[2];
  times[0].tv_sec = 987654321;
  times[0].tv_nsec = 0;
  times[1].tv_sec = 987654321;
  times[1].tv_nsec = 0;

  int result =
      utimensat(AT_FDCWD, link_path.c_str(), times, AT_SYMLINK_NOFOLLOW);
  ASSERT_EQ(0, result) << "utimensat with AT_SYMLINK_NOFOLLOW failed: "
                       << strerror(errno);

  time_t symlink_mtime_after = GetLinkMTime(link_path.c_str());
  EXPECT_EQ(987654321, symlink_mtime_after)
      << "Symlink modification time was not updated correctly.";

  time_t original_file_mtime_after = GetMTime(target_file.filename().c_str());
  EXPECT_EQ(original_file_mtime_before, original_file_mtime_after)
      << "Original file's timestamp should not have changed.";

  unlink(link_path.c_str());
}

TEST(PosixUtimensatTest, FailsOnNonExistentPath) {
  int result = utimensat(AT_FDCWD, "this_path_does_not_exist", NULL, 0);
  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOENT, errno);
}

TEST(PosixUtimensatTest, FailsOnEmptyPath) {
  int result = utimensat(AT_FDCWD, "", NULL, 0);
  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOENT, errno);
}

TEST(PosixUtimensatTest, FailsOnPathComponentNotADirectory) {
  ScopedRandomFile file_as_dir;
  std::string path = file_as_dir.filename() + "/some_file";

  int result = utimensat(AT_FDCWD, path.c_str(), NULL, 0);
  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOTDIR, errno);
}

TEST(PosixUtimensatTest, FailsOnInvalidFlag) {
  ScopedRandomFile file;
  const int INVALID_FLAG = -1;
  int result = utimensat(AT_FDCWD, file.filename().c_str(), NULL, INVALID_FLAG);
  EXPECT_EQ(-1, result) << "Function should fail with an invalid flag.";
  EXPECT_EQ(EINVAL, errno)
      << "errno should be set to EINVAL for an invalid flag.";
}

}  // namespace
}  // namespace nplb
