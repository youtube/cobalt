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
#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Global flag for signal handler
volatile sig_atomic_t g_alarm_received = 0;

// Global pipe write FD for reliable signal notification
static int g_alarm_pipe_write_fd = -1;

void AlarmSignalHandler(int signum) {
  if (signum == SIGALRM) {
    g_alarm_received = 1;
    if (g_alarm_pipe_write_fd != -1) {
      char byte = 1;
      std::ignore = write(g_alarm_pipe_write_fd, &byte, 1);
    }
  }
}

class PosixAlarmTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_alarm_received = 0;
    g_alarm_pipe_write_fd = -1;

    // Set up pipe for reliable waiting
    ASSERT_EQ(pipe(pipe_fds_), 0);
    int flags = fcntl(pipe_fds_[1], F_GETFL);
    ASSERT_NE(flags, -1);
    ASSERT_NE(fcntl(pipe_fds_[1], F_SETFL, flags | O_NONBLOCK), -1);
    g_alarm_pipe_write_fd = pipe_fds_[1];

    epoll_fd_ = epoll_create1(0);
    ASSERT_NE(epoll_fd_, -1);

    struct epoll_event event = {};
    event.events = EPOLLIN;
    event.data.fd = pipe_fds_[0];
    ASSERT_EQ(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pipe_fds_[0], &event), 0);

    // Save original SIGALRM action
    ASSERT_EQ(sigaction(SIGALRM, nullptr, &original_sa_), 0);

    // Make sure SIGALRM is unblocked
    sigset_t unblock_mask;
    sigemptyset(&unblock_mask);
    sigaddset(&unblock_mask, SIGALRM);
    ASSERT_EQ(sigprocmask(SIG_UNBLOCK, &unblock_mask, &original_mask_), 0);
  }

  void TearDown() override {
    // Cancel any pending alarm
    alarm(0);

    // Restore original SIGALRM action
    EXPECT_EQ(sigaction(SIGALRM, &original_sa_, nullptr), 0);

    // Restore original mask
    ASSERT_EQ(sigprocmask(SIG_SETMASK, &original_mask_, nullptr), 0);

    // Clean up FDs
    if (pipe_fds_[0] != -1) {
      close(pipe_fds_[0]);
    }
    if (pipe_fds_[1] != -1) {
      close(pipe_fds_[1]);
    }
    if (epoll_fd_ != -1) {
      close(epoll_fd_);
    }
    g_alarm_pipe_write_fd = -1;
  }

  int pipe_fds_[2] = {-1, -1};
  int epoll_fd_ = -1;
  struct sigaction original_sa_;
  sigset_t original_mask_;
};

// Test 1: Setting an alarm and waiting for it to fire.
TEST_F(PosixAlarmTest, AlarmFires) {
  struct sigaction sa = {};
  sa.sa_handler = AlarmSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGALRM, &sa, nullptr), 0);

  // Set alarm for 1 second
  EXPECT_EQ(alarm(1), 0u);  // Should return 0 as no alarm was set

  // Wait for alarm. 1 second is 1000ms. We wait slightly longer to be safe.
  sleep(2);
  EXPECT_EQ(g_alarm_received, 1);
}

// Test 2: alarm(0) cancels a pending alarm.
TEST_F(PosixAlarmTest, AlarmCancel) {
  struct sigaction sa = {};
  sa.sa_handler = AlarmSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGALRM, &sa, nullptr), 0);

  // Set alarm for 2 seconds
  EXPECT_EQ(alarm(2), 0u);

  // Cancel alarm immediately
  // It should return the remaining time (which should be 1 or 2 depending on
  // timing, but definitely > 0 if we do it immediately).
  unsigned int remaining = alarm(0);
  EXPECT_GT(remaining, 0u);
  EXPECT_LE(remaining, 2u);

  // Wait for the original time to ensure it doesn't fire
  sleep(2);
  EXPECT_EQ(g_alarm_received, 0);
}

// Test 3: Setting a new alarm cancels the previous one and returns remaining
// time.
TEST_F(PosixAlarmTest, AlarmOverwrites) {
  struct sigaction sa = {};
  sa.sa_handler = AlarmSignalHandler;
  sigemptyset(&sa.sa_mask);
  ASSERT_EQ(sigaction(SIGALRM, &sa, nullptr), 0);

  // Set alarm for 5 seconds
  EXPECT_EQ(alarm(5), 0u);

  // Overwrite with alarm for 1 second
  unsigned int remaining = alarm(1);
  EXPECT_GT(remaining, 0u);
  EXPECT_LE(remaining, 5u);

  // The 1-second alarm should fire
  sleep(2);
  EXPECT_EQ(g_alarm_received, 1);
}

// Test 4: alarm returns 0 if no alarm was scheduled.
TEST_F(PosixAlarmTest, AlarmReturnsZeroIfNoPrevious) {
  EXPECT_EQ(alarm(0), 0u);
  EXPECT_EQ(alarm(10), 0u);
  // Cancel it and it should return > 0
  EXPECT_GT(alarm(0), 0u);
}

}  // namespace
}  // namespace nplb
