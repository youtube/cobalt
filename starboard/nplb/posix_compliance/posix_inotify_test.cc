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

class PosixInotifyTest : public ::testing::Test {
 protected:
  // The spec requires that all watches are freed when the inotify file
  // descriptor is closed, so we only need to track the fds to close.
  std::vector<int> fds_to_close_;
  PosixInotifyTest() { errno = 0; }
  ~PosixInotifyTest() {
    for (int fd : fds_to_close_) {
      if (fd >= 0) {
        close(fd);
      }
    }
  }
};

TEST_F(PosixInotifyTest, InotifyInit) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  EXPECT_EQ(errno, 0);
  EXPECT_GE(fd, 0);
}

TEST_F(PosixInotifyTest, InotifyInit1ValidFlags) {
  int fd = inotify_init1(IN_NONBLOCK);
  fds_to_close_.push_back(fd);
  EXPECT_EQ(errno, 0);
  EXPECT_GE(fd, 0);
}

TEST_F(PosixInotifyTest, InotifyInit1InvalidFlags) {
  int fd = inotify_init1(0xFFFF);
  fds_to_close_.push_back(fd);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixInotifyTest, InotifyAddWatchValidMask) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, "/dev/null", IN_MODIFY);
  EXPECT_EQ(errno, 0);
  EXPECT_GE(wd, 0);
}

TEST_F(PosixInotifyTest, InotifyAddWatchInvalidMask) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, "/dev/null", 0);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixInotifyTest, InotifyAddWatchInvalidPath) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, "invalid_path", IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(PosixInotifyTest, InotifyAddWatchBadFd) {
  int wd = inotify_add_watch(-1, "/dev/null", IN_MODIFY);
  EXPECT_EQ(wd, -1);
  EXPECT_EQ(errno, EBADF);
}

TEST_F(PosixInotifyTest, InotifyRmWatch) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  ASSERT_GE(fd, 0);

  int wd = inotify_add_watch(fd, "/dev/null", IN_MODIFY);
  ASSERT_GE(wd, 0);

  EXPECT_EQ(inotify_rm_watch(fd, wd), 0);
  EXPECT_EQ(errno, 0);
}

TEST_F(PosixInotifyTest, InotifyRmWatchBadFd) {
  EXPECT_EQ(inotify_rm_watch(-1, 0), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST_F(PosixInotifyTest, InotifyRmWatchBadWatchDescriptor) {
  int fd = inotify_init();
  fds_to_close_.push_back(fd);
  ASSERT_GE(fd, 0);

  EXPECT_EQ(inotify_rm_watch(fd, -1), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(PosixInotifyTest, WatchedFileTriggersNotification) {
  nplb::ScopedRandomFile file;
  ASSERT_TRUE(nplb::FileExists(file.filename().c_str()));

  int inotify_fd = inotify_init1(0);
  ASSERT_GE(inotify_fd, 0);

  int wd = inotify_add_watch(inotify_fd, file.filename().c_str(), IN_MODIFY);
  ASSERT_GE(wd, 0);

  const char* test_data = "test data";
  int file_fd = open(file.filename().c_str(), O_WRONLY);
  fds_to_close_.push_back(file_fd);
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
}

}  // namespace
