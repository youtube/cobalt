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

// Some behaviors from https://man7.org/linux/man-pages/man2/eventfd.2.html are
// not tested:
// - The poll use case is just tested via poll(). It seems like overkill to also
//   test the analogous behavior via select() and epoll().
// - poll() should return a POLLERR event if an overflow of the counter value is
//   detected. There's not a good way to set up this scenario since write() is
//   not permitted to overflow the counter.
// - We don't have a good way to set up the system state required to reliably
//   test the ENFILE, ENODEV, or ENOMEM error cases.

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

struct TestContext {
  TestContext(int fd, uint64_t value, int64_t delay_us)
      : fd(fd), value(value), delay_us(delay_us) {}
  int fd;
  uint64_t value;
  int64_t delay_us;
};

constexpr uint64_t kMaxCounterValue = 0xfffffffffffffffe;  // Defined by spec

// Leverages eventfd's poll operation to check whether the associated counter
// has a zero value. Note that there are several test cases dedicated to
// testing the poll operation, itself.
bool IsCounterZero(int fd) {
  struct pollfd poll_fd = {0};
  poll_fd.fd = fd;
  poll_fd.events = POLLIN;

  int timeout_millis = 10;  // A somewhat arbitrary but short timeout
  if (!poll(&poll_fd, 1, timeout_millis)) {
    return true;
  }

  if ((poll_fd.revents & POLLIN) == POLLIN) {
    return false;
  } else {
    return true;
  }
}

TEST(PosixEventfdTest, NonSemaphoreReadNonzeroCounterReturnsCounter) {
  uint64_t counter_value = 5;
  int fd = eventfd(counter_value, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, counter_value);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, NonSemaphoreReadNonzeroCounterResetsCounterToZero) {
  uint64_t initial_counter_value = 5;
  int fd = eventfd(initial_counter_value, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  EXPECT_FALSE(IsCounterZero(fd));

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);

  EXPECT_TRUE(IsCounterZero(fd));

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, SemaphoreReadNonzeroCounterReturnsValueOne) {
  uint64_t counter_value = 5;
  uint64_t expected_read_value = 1;
  int fd = eventfd(counter_value, EFD_SEMAPHORE);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, expected_read_value);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, SemaphoreReadNonzeroCounterDecrementsCounterByOne) {
  uint64_t initial_counter_value = 2;
  int fd = eventfd(initial_counter_value, EFD_SEMAPHORE);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  EXPECT_FALSE(IsCounterZero(fd));  // Counter should be 2

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);

  EXPECT_FALSE(IsCounterZero(fd));  // Counter should be 1

  read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);

  EXPECT_TRUE(IsCounterZero(fd));  // Counter should be 0

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, WriteAddsValueToCounter) {
  uint64_t initial_counter_value = 0;
  int fd = eventfd(initial_counter_value, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t write_value = 5;
  ssize_t written_bytes = write(fd, &write_value, sizeof(write_value));
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(write_value));

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, initial_counter_value + write_value);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, ReadBufferLessThanEightBytesFailsWithEINVAL) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint8_t two_byte_read_buffer;
  EXPECT_EQ(read(fd, &two_byte_read_buffer, sizeof(two_byte_read_buffer)), -1);
  EXPECT_EQ(errno, EINVAL);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, WriteBufferLessThanEightBytesFailsWithEINVAL) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint8_t two_byte_write_value = 5;
  EXPECT_EQ(write(fd, &two_byte_write_value, sizeof(two_byte_write_value)), -1);
  EXPECT_EQ(errno, EINVAL);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, WriteMaxUint64_tFailsWithEINVAL) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t max_uint64_t = 0xffffffffffffffff;
  EXPECT_EQ(write(fd, &max_uint64_t, sizeof(max_uint64_t)), -1);
  EXPECT_EQ(errno, EINVAL);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, PollFdIsReadableIfCounterGreaterThanZero) {
  int fd = eventfd(1, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  struct pollfd poll_fd = {0};
  poll_fd.fd = fd;
  poll_fd.events = POLLIN;

  int timeout_millis = 10;
  // TODO: b/412653190 - we should use epoll() to test the poll use case if we
  // decide to remove poll() from the hermetic build.
  EXPECT_EQ(poll(&poll_fd, 1, timeout_millis), 1);
  EXPECT_EQ(poll_fd.revents & POLLIN, POLLIN);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, PollFdIsNotReadableIfCounterHasZeroValue) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  struct pollfd poll_fd = {0};
  poll_fd.fd = fd;
  poll_fd.events = POLLIN;

  int timeout_millis = 10;
  EXPECT_EQ(poll(&poll_fd, 1, timeout_millis), 0);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest,
     PollFdIsWritableIfCanWriteValueOfAtLeastOneWithoutBlocking) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t counter_value = kMaxCounterValue - 1;  // Space to write value of "1"
  ssize_t written_bytes = write(fd, &counter_value, sizeof(counter_value));
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(counter_value));

  struct pollfd poll_fd = {0};
  poll_fd.fd = fd;
  poll_fd.events = POLLOUT;

  int timeout_millis = 10;
  EXPECT_EQ(poll(&poll_fd, 1, timeout_millis), 1);
  EXPECT_EQ(poll_fd.revents & POLLOUT, POLLOUT);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest,
     PollFdIsNotWritableIfCannotWriteValueOfAtLeastOneWithoutBlocking) {
  // We can't initialize the counter to its maxiumum value - the largest
  // unsigned 64-bit value minus 1 (i.e., 0xfffffffffffffffe) - because the
  // initval parameter is only an unsigned int. So we initialize the counter to
  // zero and then write 0xfffffffffffffffe to configure the counter such that
  // any additional write would exceed the counter's maximum.
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  ssize_t written_bytes = write(fd, &kMaxCounterValue, sizeof(kMaxCounterValue));
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(kMaxCounterValue));

  struct pollfd poll_fd = {0};
  poll_fd.fd = fd;
  poll_fd.events = POLLOUT;

  int timeout_millis = 10;
  EXPECT_EQ(poll(&poll_fd, 1, timeout_millis), 0);

  EXPECT_EQ(close(fd), 0);
}


// TODO: b/412648662 - If/when fork() and an exec family function are added to
// the hermetic build, we should consider adding a test to verify that the file
// descriptor is actually closed in the child process after execution of an exec
// function.
TEST(PosixEventfdTest, EventfdWithEFD_CLOEXECSetsFlagOnNewFileDescriptor) {
  int fd = eventfd(0, EFD_CLOEXEC);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  int descriptor_flags = fcntl(fd, F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, EventfdWithEFD_NONBLOCKSetsFlagOnOpeFileDescription) {
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  int file_status_flags = fcntl(fd, F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, EventfdWitMultipleFlagsSetsExpectedFlagsForFd) {
  int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  int descriptor_flags = fcntl(fd, F_GETFD);
  EXPECT_NE(descriptor_flags, -1);
  EXPECT_TRUE(descriptor_flags & FD_CLOEXEC);

  int file_status_flags = fcntl(fd, F_GETFL);
  EXPECT_NE(file_status_flags, -1);
  EXPECT_TRUE(file_status_flags & O_NONBLOCK);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, ReadZeroValueCounterFailsWithEAGAINIfFdIsNonblocking) {
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t read_value;
  EXPECT_EQ(read(fd, &read_value, sizeof(read_value)), -1);
  EXPECT_EQ(errno, EAGAIN);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest,
     ReadZeroValueFromSemaphoreCounterFailsWithEAGAINIfFdIsNonblocking) {
  int fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t read_value;
  EXPECT_EQ(read(fd, &read_value, sizeof(read_value)), -1);
  EXPECT_EQ(errno, EAGAIN);

  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest,
     WriteToExceedMaxCounterValueFailsWithEAGAINIfFdIsNonblocking) {
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  ssize_t written_bytes = write(fd, &kMaxCounterValue,
                                sizeof(kMaxCounterValue));
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(kMaxCounterValue));

  uint64_t overflow_value = 1;
  EXPECT_EQ(write(fd, &overflow_value, sizeof(overflow_value)), -1);
  EXPECT_EQ(errno, EAGAIN);

  EXPECT_EQ(close(fd), 0);
}

void* DoDelayedWrite(void* test_context) {
  TestContext* context = static_cast<TestContext*>(test_context);

  usleep(context->delay_us);

  ssize_t written_bytes = write(context->fd, &context->value,
                                sizeof(context->value));
  EXPECT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(context->value));

  return nullptr;
}

TEST(PosixEventfdTest,
     ReadZeroValueCounterBlocksUntilNonzeroIfFdIsNotNonblocking) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  uint64_t nonzero_counter_value = 99;
  // 10 ms should ensure the secondary thread writes to the file descriptor, to
  // make the counter nonzero, after the main thread has blocked on a read.
  int64_t delay_us = 10'000;
  TestContext context(fd, nonzero_counter_value, delay_us);
  pthread_t write_thread;
  ASSERT_EQ(pthread_create(&write_thread, NULL, DoDelayedWrite, &context), 0);

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));  // Blocks
  ASSERT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, nonzero_counter_value);

  EXPECT_EQ(pthread_join(write_thread, NULL), 0);
  EXPECT_EQ(close(fd), 0);
}

void* DoDelayedRead(void* test_context) {
  TestContext* context = static_cast<TestContext*>(test_context);

  usleep(context->delay_us);

  uint64_t read_value;
  ssize_t read_bytes = read(context->fd, &read_value, sizeof(read_value));
  EXPECT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, context->value);

  return nullptr;
}

TEST(PosixEventfdTest,
     WriteToExceedMaxCounterValueBlocksUntilFdIsReadIfFdIsNonblocking) {
  int fd = eventfd(0, 0);
  ASSERT_NE(fd, -1) << "eventfd() failed";

  ssize_t written_bytes = write(fd, &kMaxCounterValue,
                                sizeof(kMaxCounterValue));
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(kMaxCounterValue));

  uint64_t expected_read_value = kMaxCounterValue;
  // 10 ms should ensure the secondary thread reads from the file descriptor
  // after the main thread has blocked on a write.
  int64_t delay_us = 10'000;
  TestContext context(fd, expected_read_value, delay_us);
  pthread_t read_thread;
  ASSERT_EQ(pthread_create(&read_thread, NULL, DoDelayedRead, &context), 0);

  uint64_t overflow_value = 99;
  written_bytes = write(fd, &overflow_value, sizeof(overflow_value));  // Blocks
  ASSERT_GE(written_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(written_bytes), sizeof(overflow_value));

  uint64_t read_value;
  ssize_t read_bytes = read(fd, &read_value, sizeof(read_value));
  ASSERT_GE(read_bytes, 0);
  EXPECT_EQ(static_cast<size_t>(read_bytes), sizeof(read_value));
  EXPECT_EQ(read_value, overflow_value);

  EXPECT_EQ(pthread_join(read_thread, NULL), 0);
  EXPECT_EQ(close(fd), 0);
}

TEST(PosixEventfdTest, EventfdWithUnsupportedFlagFailsWithEINVAL) {
  EXPECT_EQ(eventfd(0, -1), -1) << "eventfd() failed";
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixEventfdTest,
     DISABLED_EventfdFailsWithEMFILEIfTooManyFileDescriptorsOpen) {
  // TODO: b/412648662 - If/when getrlimit() and setrimit() are added to the
  // hermetic build, use them to drop the maximum value that the system can
  // assign to new file descriptors for this process to just above the current
  // usage in order to easily force this scenario.
  FAIL() << "This test requires the ability to manipulate process resource "
         << "limits (rlimit) to reliably trigger EMFILE, which is not "
         << "currently available in this test environment.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
