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

/* The following error scenarios are untested for epoll_wait():
1. EFAULT (events pointer is bad) is difficult to safely and reliably test.
2. EINTR (interrupted by signal) is difficult to test.
3. Negative timeout is an infinite timeout, which is difficult to test.
*/

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "starboard/nplb/posix_compliance/posix_epoll_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixEpollWaitTests : public PosixEpollTest {
 protected:
  int epfd_ = -1;
  int pipe_fds_[2] = {-1, -1};  // pipe_fds_[0] for read, pipe_fds_[1] for write
  struct epoll_event events_[kMaxEvents];

  void SetUp() override {
    epfd_ = epoll_create(1);
    ASSERT_NE(epfd_, -1) << "Failed to create epoll instance in SetUp: "
                         << strerror(errno);
    // Use non-blocking pipes to prevent writes from blocking indefinitely if
    // pipe is full
    ASSERT_TRUE(CreatePipe(pipe_fds_, O_NONBLOCK))
        << "Failed to create pipe in SetUp: " << strerror(errno);
  }

  void TearDown() override {
    if (epfd_ >= 0) {
      close(epfd_);
      epfd_ = -1;
    }
    if (pipe_fds_[0] >= 0) {
      close(pipe_fds_[0]);
      pipe_fds_[0] = -1;
    }
    if (pipe_fds_[1] >= 0) {
      close(pipe_fds_[1]);
      pipe_fds_[1] = -1;
    }
  }

  // Helper to add to epoll
  void AddToEpoll(int fd_to_add, uint32_t ev_mask) {
    struct epoll_event event = {};
    event.events = ev_mask;
    event.data.fd = fd_to_add;
    ASSERT_EQ(epoll_ctl(epfd_, EPOLL_CTL_ADD, fd_to_add, &event), 0)
        << "Failed to add fd " << fd_to_add << " to epoll with events "
        << ev_mask << ": " << strerror(errno);
  }

  // Helper to remove from epoll
  void RemoveFromEpoll(int fd_to_remove) {
    struct epoll_event event{};  // Dummy event, required for older kernels
    if (epfd_ >= 0 && fd_to_remove >= 0) {
      epoll_ctl(epfd_, EPOLL_CTL_DEL, fd_to_remove, &event);
    }
  }
};

TEST_F(PosixEpollWaitTests, EventReportedSuccessfully) {
  AddToEpoll(pipe_fds_[0], EPOLLIN);  // Watch read end for input

  char buffer = 'x';
  const size_t buffer_length = sizeof(buffer);
  ASSERT_EQ(static_cast<size_t>(write(pipe_fds_[1], &buffer, buffer_length)),
            buffer_length)
      << "Write to pipe failed: " << strerror(errno);

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, kModerateTimeoutMs);
  ASSERT_EQ(nfds, 1) << "epoll_wait did not return 1 event: " << strerror(errno)
                     << " (nfds=" << nfds << ")";
  ASSERT_EQ(events_[0].data.fd, pipe_fds_[0]);
  ASSERT_TRUE(events_[0].events & EPOLLIN);
  RemoveFromEpoll(pipe_fds_[0]);
}

TEST_F(PosixEpollWaitTests, TimeoutZeroReturnsImmediately) {
  AddToEpoll(pipe_fds_[0], EPOLLIN);

  // Ensure no events are pending initially by draining the pipe
  char buffer[10];
  while (read(pipe_fds_[0], buffer, sizeof(buffer)) > 0) {
  }  // Drain

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, 0);  // 0ms timeout
  ASSERT_EQ(nfds, 0) << "epoll_wait with 0 timeout did not return 0: "
                     << strerror(errno);
  RemoveFromEpoll(pipe_fds_[0]);
}

TEST_F(PosixEpollWaitTests, TimeoutPositiveReturnsAfterTimeout) {
  AddToEpoll(pipe_fds_[0], EPOLLIN);

  // Ensure no events are pending initially
  char buffer[10];
  while (read(pipe_fds_[0], buffer, sizeof(buffer)) > 0) {
  }  // Drain

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, kShortTimeoutMs);
  ASSERT_EQ(nfds, 0)
      << "epoll_wait with positive timeout did not return 0 (event occurred?): "
      << strerror(errno);
  RemoveFromEpoll(pipe_fds_[0]);
}

TEST_F(PosixEpollWaitTests, MaxEventsLimitsReturnedEvents) {
  int p2[2] = {-1, -1};
  ASSERT_TRUE(CreatePipe(p2, O_NONBLOCK));
  int pipe2_read_fd = p2[0];
  int pipe2_write_fd = p2[1];

  AddToEpoll(pipe_fds_[0], EPOLLIN);
  AddToEpoll(pipe2_read_fd, EPOLLIN);

  char buffer = 'z';
  ASSERT_EQ(write(pipe_fds_[1], &buffer, 1), 1);
  ASSERT_EQ(write(pipe2_write_fd, &buffer, 1), 1);

  const int kMaxEventsForThisTest = 1;
  struct epoll_event single_event[kMaxEventsForThisTest];
  int nfds = epoll_wait(epfd_, single_event, kMaxEventsForThisTest,
                        kModerateTimeoutMs);
  ASSERT_EQ(nfds, kMaxEventsForThisTest)
      << "epoll_wait did not return max_events: " << strerror(errno);

  RemoveFromEpoll(pipe_fds_[0]);
  RemoveFromEpoll(pipe2_read_fd);

  if (pipe2_read_fd >= 0) {
    close(pipe2_read_fd);
  }
  if (pipe2_write_fd >= 0) {
    close(pipe2_write_fd);
  }
}

TEST_F(PosixEpollWaitTests, ErrorBadEpfd) {
  ASSERT_EQ(epoll_wait(kInvalidFd, events_, kMaxEvents, 0), -1);
  ASSERT_EQ(errno, EBADF);
}

TEST_F(PosixEpollWaitTests, ErrorInvalidMaxEventsZero) {
  ASSERT_EQ(epoll_wait(epfd_, events_, 0, 0), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollWaitTests, ErrorInvalidMaxEventsNegative) {
  ASSERT_EQ(epoll_wait(epfd_, events_, -1, 0), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollWaitTests, ErrorEpfdIsNotEpollFd) {
  // Use one end of the pipe as a non-epoll fd for epfd argument
  // (pipe_fds_[1] is valid from SetUp)
  ASSERT_EQ(epoll_wait(pipe_fds_[1], events_, kMaxEvents, 0), -1);
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(PosixEpollWaitTests, WaitForWriteOnReadOnlyPipeEndNeverSignalsWritable) {
  // pipe_fds_[0] is the read end, inherently not writable.
  // We expect a timeout, not EPOLLOUT.
  AddToEpoll(pipe_fds_[0], EPOLLOUT);

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, kShortTimeoutMs);

  ASSERT_EQ(nfds, 0)
      << "epoll_wait should timeout for EPOLLOUT on read-only pipe end. "
      << "nfds=" << nfds << ", errno=" << (errno ? strerror(errno) : "none")
      << (nfds > 0 ? ", event=" + std::to_string(events_[0].events) : "");
  if (nfds > 0) {
    ASSERT_FALSE(events_[0].events & EPOLLOUT)
        << "EPOLLOUT incorrectly signalled for read-only pipe end.";
  }
  RemoveFromEpoll(pipe_fds_[0]);
}

TEST_F(PosixEpollWaitTests, WaitForReadOnWriteOnlyPipeEndNeverSignalsReadable) {
  // pipe_fds_[1] is the write end, inherently not readable.
  // We expect a timeout, not EPOLLIN.
  AddToEpoll(pipe_fds_[1], EPOLLIN);

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, kShortTimeoutMs);

  ASSERT_EQ(nfds, 0)
      << "epoll_wait should timeout for EPOLLIN on write-only pipe end. "
      << "nfds=" << nfds << ", errno=" << (errno ? strerror(errno) : "none")
      << (nfds > 0 ? ", event=" + std::to_string(events_[0].events) : "");
  if (nfds > 0) {
    ASSERT_FALSE(events_[0].events & EPOLLIN)
        << "EPOLLIN incorrectly signalled for write-only pipe end.";
  }
  RemoveFromEpoll(pipe_fds_[1]);
}

TEST_F(PosixEpollWaitTests, WaitForWriteOnReadOnlyPipeEndWhenWriteEndClosed) {
  AddToEpoll(pipe_fds_[0], EPOLLIN | EPOLLOUT | EPOLLHUP);

  ASSERT_NE(pipe_fds_[1], -1);  // Ensure write end is initially valid
  close(pipe_fds_[1]);
  pipe_fds_[1] = -1;  // Mark as closed for TearDown logic

  int nfds = epoll_wait(epfd_, events_, kMaxEvents, kModerateTimeoutMs);
  ASSERT_GE(nfds, 0) << "epoll_wait failed: " << strerror(errno);

  bool hup_received = false;
  bool out_received = false;
  bool in_received_for_eof = false;

  if (nfds > 0) {
    ASSERT_EQ(events_[0].data.fd, pipe_fds_[0]);
    if (events_[0].events & EPOLLIN) {
      in_received_for_eof = true;
    }
    if (events_[0].events & EPOLLHUP) {
      hup_received = true;
    }
    if (events_[0].events & EPOLLOUT) {
      out_received = true;
    }

    ASSERT_TRUE(in_received_for_eof || hup_received)
        << "Expected EPOLLIN (EOF) or EPOLLHUP on read end when write end "
           "closed. Events: "
        << events_[0].events;
    ASSERT_FALSE(out_received)
        << "EPOLLOUT incorrectly signalled for read-only pipe end when write "
           "end closed. Events: "
        << events_[0].events;
  } else {
    char c_buf;
    // Attempt a read to ensure EOF is processed by the kernel for pipe_fds_[0]
    // This should see EOF (return 0) or error.
    std::ignore = read(pipe_fds_[0], &c_buf, 1);

    nfds = epoll_wait(epfd_, events_, kMaxEvents, kModerateTimeoutMs);
    ASSERT_GT(nfds, 0) << "No event reported on read-end after write-end "
                          "closed and explicit read attempt. Errno: "
                       << (errno ? strerror(errno) : "none");
    if (nfds > 0) {
      ASSERT_EQ(events_[0].data.fd, pipe_fds_[0]);
      if (events_[0].events & EPOLLIN) {
        in_received_for_eof = true;
      }
      if (events_[0].events & EPOLLHUP) {
        hup_received = true;
      }
      if (events_[0].events & EPOLLOUT) {
        out_received = true;
      }

      // After a read attempt on a pipe with a closed write end,
      // EPOLLIN (due to EOF) and EPOLLHUP are expected.
      ASSERT_TRUE(in_received_for_eof && hup_received)
          << "Expected EPOLLIN (EOF) and EPOLLHUP after read attempt. Events: "
          << events_[0].events;
      ASSERT_FALSE(out_received)
          << "EPOLLOUT incorrectly signalled for read-only pipe end when write "
             "end closed. Events: "
          << events_[0].events;
    }
  }
  RemoveFromEpoll(pipe_fds_[0]);
}

}  // namespace
}  // namespace nplb
