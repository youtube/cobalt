// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(PosixInotifyTest, InotifyInit) {
  errno = 0;
  int fd = inotify_init();
  EXPECT_EQ(errno, 0);
  EXPECT_GE(fd, 0);
  close(fd);
}

TEST(PosixInotifyTest, InotifyInit1ValidFlags) {
  errno = 0;
  int fd = inotify_init1(IN_NONBLOCK);
  EXPECT_EQ(errno, 0);
  EXPECT_GE(fd, 0);
  close(fd);
}

TEST(PosixInotifyTest, InotifyInit1InvalidFlags) {
  errno = 0;
  // Set all flag bits except the valid ones.
  int fd = inotify_init1(~(IN_NONBLOCK | IN_CLOEXEC));
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EINVAL);
  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchValidMask) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  // errno may have been set to EEXIST indirectly by the constructor of
  // ScopedRandomFile.
  errno = 0;
  int wd = inotify_add_watch(fd, file.filename().c_str(), IN_MODIFY);
  EXPECT_EQ(errno, 0);
  EXPECT_GE(wd, 0);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchInvalidMask) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd = inotify_add_watch(fd, file.filename().c_str(), 0);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EINVAL);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchInvalidPath) {
  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, "invalid_path", IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, ENOENT);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchBadFd) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  errno = 0;
  int wd = inotify_add_watch(-1, file.filename().c_str(), IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixInotifyTest, InotifyAddWatchEinvalNotAnInotifyFd) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = open(file.filename().c_str(), O_RDONLY);
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd = inotify_add_watch(fd, file.filename().c_str(), IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EINVAL);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchEexist) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  int wd1 = inotify_add_watch(fd, file.filename().c_str(), IN_MODIFY);
  ASSERT_GE(wd1, 0);

  errno = 0;
  int wd2 = inotify_add_watch(fd, file.filename().c_str(),
                              IN_MODIFY | IN_MASK_CREATE);
  // IN_MASK_CREATE is not present in kernels <4.18 and will cause flag
  // validation to fail with EINVAL.
  if (errno == EINVAL) {
    GTEST_SKIP() << "IN_MASK_CREATE is not defined on this platform";
  }
  EXPECT_EQ(wd2, -1);
  EXPECT_EQ(errno, EEXIST);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchEnoentEmptyPath) {
  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd = inotify_add_watch(fd, "", IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, ENOENT);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchEnoentMissingDirectory) {
  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd = inotify_add_watch(fd, "/nonexistent_directory/file", IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, ENOENT);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchEnotdir) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd =
      inotify_add_watch(fd, file.filename().c_str(), IN_MODIFY | IN_ONLYDIR);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, ENOTDIR);

  close(fd);
}

TEST(PosixInotifyTest, InotifyAddWatchEinvalAddAndCreateFlags) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  errno = 0;
  int wd = inotify_add_watch(fd, file.filename().c_str(),
                             IN_MODIFY | IN_MASK_ADD | IN_MASK_CREATE);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EINVAL);

  close(fd);
}

TEST(PosixInotifyTest, InotifyRmWatch) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, file.filename().c_str(), IN_MODIFY);
  ASSERT_GE(wd, 0);

  errno = 0;
  EXPECT_EQ(inotify_rm_watch(fd, wd), 0);
  EXPECT_EQ(errno, 0);

  close(fd);
}

TEST(PosixInotifyTest, InotifyRmWatchBadFd) {
  EXPECT_EQ(inotify_rm_watch(-1, 0), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixInotifyTest, InotifyRmWatchBadWatchDescriptor) {
  int fd = inotify_init();
  ASSERT_GE(fd, 0);

  EXPECT_EQ(inotify_rm_watch(fd, -1), -1);
  EXPECT_EQ(errno, EINVAL);

  close(fd);
}

TEST(PosixInotifyTest, InotifyRmWatchEinvalNotAnInotifyFd) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int fd = open(file.filename().c_str(), O_RDONLY);
  ASSERT_GE(fd, 0);

  errno = 0;
  EXPECT_EQ(inotify_rm_watch(fd, 1), -1);
  EXPECT_EQ(errno, EINVAL);

  close(fd);
}

TEST(PosixInotifyTest, WatchedFileTriggersNotification) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int inotify_fd = inotify_init1(0);
  ASSERT_GE(inotify_fd, 0);

  int wd = inotify_add_watch(inotify_fd, file.filename().c_str(), IN_MODIFY);
  ASSERT_GE(wd, 0);

  const char* test_data = "test data";
  int file_fd = open(file.filename().c_str(), O_WRONLY);
  ASSERT_GE(file_fd, 0);

  ssize_t bytes_written = write(file_fd, test_data, strlen(test_data));
  ASSERT_EQ(static_cast<size_t>(bytes_written), strlen(test_data));

  alignas(struct inotify_event) char
      event_buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
  ssize_t len = read(inotify_fd, event_buffer, sizeof(event_buffer));
  ASSERT_GT(len, 0) << "Failed to read inotify event: " << strerror(errno);

  struct inotify_event* event =
      reinterpret_cast<struct inotify_event*>(event_buffer);
  EXPECT_EQ(event->wd, wd);
  EXPECT_TRUE(event->mask & IN_MODIFY);

  close(file_fd);
  close(inotify_fd);
}

}  // namespace
