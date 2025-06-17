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

// The following scenario is not reliably testable for pipe() and pipe2():
// - ENFILE: This error indicates the system file table is full, which is
//   difficult to trigger from a user-space test without impacting the system
//   or requiring specific, non-standard system configurations.

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Only used by the WriteToFullPipeWithNonBlockFlagImmediatelyReturnsError test.
struct TestContext {
  TestContext(int read_fd, int timeout_s)
      : out_of_time(false), read_fd(read_fd), timeout_s(timeout_s) {
    EXPECT_EQ(pthread_mutex_init(&out_of_time_mutex, NULL), 0);
  }
  bool out_of_time;
  pthread_mutex_t out_of_time_mutex;
  int read_fd;
  int timeout_s;
};

const char kTestData[] = "Hello, POSIX Pipe!";
const size_t kTestDataSize = sizeof(kTestData);  // Include null terminator

TEST(PosixPipeTest, PipePlacesTwoValidFileDescriptorsIntoFildesArray) {
  int pipe_fds[2];

  EXPECT_EQ(pipe(pipe_fds), 0);

  EXPECT_GE(pipe_fds[0], 0);
  EXPECT_GE(pipe_fds[1], 0);
  EXPECT_EQ(close(pipe_fds[0]), 0);
  EXPECT_EQ(close(pipe_fds[1]), 0);
}

TEST(PosixPipeTest, Pipe2PlacesTwoValidFileDescriptorsIntoFildesArray) {
  int pipe_fds[2];
  int arbitrary_valid_flag = O_CLOEXEC;

  EXPECT_EQ(pipe2(pipe_fds, arbitrary_valid_flag), 0);

  EXPECT_GE(pipe_fds[0], 0);
  EXPECT_GE(pipe_fds[1], 0);
  EXPECT_EQ(close(pipe_fds[0]), 0);
  EXPECT_EQ(close(pipe_fds[1]), 0);
}

TEST(PosixPipeTest, PipeLeavesCloseOnExecClearOnBothEndsOfPipe) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(flags_read_end, -1);
  EXPECT_FALSE(flags_read_end & FD_CLOEXEC);

  int flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(flags_write_end, -1);
  EXPECT_FALSE(flags_write_end & FD_CLOEXEC);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, PipeLeavesNonBlockClearOnBothEndsOfPipe) {
  int pipe_fds[2];
  ASSERT_EQ(pipe(pipe_fds), 0);

  int flags_read_end = fcntl(pipe_fds[0], F_GETFL);
  EXPECT_NE(flags_read_end, -1);
  EXPECT_FALSE(flags_read_end & O_NONBLOCK);

  int flags_write_end = fcntl(pipe_fds[1], F_GETFL);
  EXPECT_NE(flags_write_end, -1);
  EXPECT_FALSE(flags_write_end & O_NONBLOCK);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// TODO: b/412648662 - If/when fork() and an exec family function are added to
// the hermetic build, we should consider adding a test to verify that the file
// descriptors are actually closed in the child process after execution of an
// exec function.
TEST(PosixPipeTest, Pipe2WithCloseOnExecSetsThisFlagOnBothEndsOfPipe) {
  int pipe_fds[2];

  ASSERT_EQ(pipe2(pipe_fds, O_CLOEXEC), 0);

  int descriptor_flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(descriptor_flags_read_end, -1);
  EXPECT_TRUE(descriptor_flags_read_end & FD_CLOEXEC);

  int descriptor_flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(descriptor_flags_write_end, -1);
  EXPECT_TRUE(descriptor_flags_write_end & FD_CLOEXEC);

  // The state of O_NONBLOCK is determined solely by the flag argument and so
  // should not be set for read and write-end file descriptions.
  int file_status_flags_read_end = fcntl(pipe_fds[0], F_GETFL);
  EXPECT_NE(file_status_flags_read_end, -1);
  EXPECT_FALSE(file_status_flags_read_end & O_NONBLOCK);

  int file_status_flags_write_end = fcntl(pipe_fds[1], F_GETFL);
  EXPECT_NE(file_status_flags_write_end, -1);
  EXPECT_FALSE(file_status_flags_write_end & O_NONBLOCK);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, Pipe2WithNonBlockSetsThisFlagOnBothEndsOfPipe) {
  int pipe_fds[2];

  ASSERT_EQ(pipe2(pipe_fds, O_NONBLOCK), 0);

  int file_status_flags_read_end = fcntl(pipe_fds[0], F_GETFL);
  EXPECT_NE(file_status_flags_read_end, -1);
  EXPECT_TRUE(file_status_flags_read_end & O_NONBLOCK);

  int file_status_flags_write_end = fcntl(pipe_fds[1], F_GETFL);
  EXPECT_NE(file_status_flags_write_end, -1);
  EXPECT_TRUE(file_status_flags_write_end & O_NONBLOCK);

  // The state of FD_CLOEXEC is determined solely by the flag argument and so
  // should not be set for read and write-end file descriptors.
  int descriptor_flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(descriptor_flags_read_end, -1);
  EXPECT_FALSE(descriptor_flags_read_end & FD_CLOEXEC);

  int descriptor_flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(descriptor_flags_write_end, -1);
  EXPECT_FALSE(descriptor_flags_write_end & FD_CLOEXEC);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, ReadFromEmptyPipeCreatedWithNonBlockFlagDoesNotBlock) {
  int pipe_fds[2];

  ASSERT_EQ(pipe2(pipe_fds, O_NONBLOCK), 0);

  char buffer[1];
  // Attempting to read from the empty pipe should fail immediately.
  EXPECT_EQ(-1, read(pipe_fds[0], buffer, sizeof(buffer)));
  // The error should be EAGAIN, indicating the operation would have blocked.
  EXPECT_EQ(EAGAIN, errno);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

void* DoDelayedRead(void* parameter) {
  TestContext* context = static_cast<TestContext*>(parameter);

  // TODO: b/412690906 - Consider using sem_timedwait() or
  // pthread_cond_timedwait() instead so that the test can avoid a forced delay
  // in the passing case.
  sleep(context->timeout_s);
  pthread_mutex_lock(&context->out_of_time_mutex);
  context->out_of_time = true;
  pthread_mutex_unlock(&context->out_of_time_mutex);

  // Reading one page worth of bytes should be sufficient to unblock writing to
  // the pipe, and 4096 bytes should cover this. We can consider using sysconf()
  // to query for the page size if/when that symbol is added to the hermetic
  // build.
  int estimated_page_size = 4096;
  char buffer[estimated_page_size];
  EXPECT_EQ(estimated_page_size,
            read(context->read_fd, buffer, estimated_page_size));
  return NULL;
}

TEST(PosixPipeTest, WriteToFullPipeWithNonBlockFlagImmediatelyReturnsError) {
  int pipe_fds[2];

  ASSERT_EQ(pipe2(pipe_fds, O_NONBLOCK), 0);

  // This thread is used to time out the test for noncompliant platforms that
  // have blocked while writing to the pipe: the large read should provide
  // sufficient space in the pipe to unblock the main thread's write() call so
  // that the test can fail rather than hang.
  int timeout_s = 5;
  TestContext context(pipe_fds[0], timeout_s);
  pthread_t read_thread;
  EXPECT_EQ(pthread_create(&read_thread, NULL, DoDelayedRead, &context), 0);

  int write_result = 0;
  while (true) {
    write_result = write(pipe_fds[1], kTestData, kTestDataSize);

    if (write_result == -1) {
      break;  // Expected termination
    }

    pthread_mutex_lock(&context.out_of_time_mutex);
    bool out_of_time = context.out_of_time;
    pthread_mutex_unlock(&context.out_of_time_mutex);
    if (out_of_time) {
      break;  // Unexpected termination
    }
  }

  // A timed out test won't meet these expectations.
  EXPECT_EQ(write_result, -1);
  EXPECT_EQ(EAGAIN, errno);

  EXPECT_EQ(pthread_join(read_thread, NULL), 0);
  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, Pipe2WithMultipleFlagsSetsTheseFlagsOnBothEndsOfPipe) {
  int pipe_fds[2];

  ASSERT_EQ(pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK), 0);

  int descriptor_flags_read_end = fcntl(pipe_fds[0], F_GETFD);
  EXPECT_NE(descriptor_flags_read_end, -1);
  EXPECT_TRUE(descriptor_flags_read_end & FD_CLOEXEC);

  int descriptor_flags_write_end = fcntl(pipe_fds[1], F_GETFD);
  EXPECT_NE(descriptor_flags_write_end, -1);
  EXPECT_TRUE(descriptor_flags_write_end & FD_CLOEXEC);

  int file_status_flags_read_end = fcntl(pipe_fds[0], F_GETFL);
  EXPECT_NE(file_status_flags_read_end, -1);
  EXPECT_TRUE(file_status_flags_read_end & O_NONBLOCK);

  int file_status_flags_write_end = fcntl(pipe_fds[1], F_GETFL);
  EXPECT_NE(file_status_flags_write_end, -1);
  EXPECT_TRUE(file_status_flags_write_end & O_NONBLOCK);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, Pipe2WithInvalidFlagFails) {
  int pipe_fds[2];
  int invalid_flag = F_GETFD;  // A valid flag, but not for pipe2.
  EXPECT_EQ(-1, pipe2(pipe_fds, invalid_flag));
  EXPECT_EQ(EINVAL, errno);
}

// TODO: b/412648662 - If/when fork() is added to the hermetic build, we should
// also test that the pipe can actually be used to send data between processes.
TEST(PosixPipeTest, DataWrittenToPipeCanBeRead) {
  int pipe_fds[2];

  ASSERT_EQ(pipe(pipe_fds), 0);

  char read_buffer[kTestDataSize];

  ssize_t bytes_written = write(pipe_fds[1], kTestData, kTestDataSize);
  EXPECT_EQ(bytes_written, kTestDataSize);

  ssize_t bytes_read = read(pipe_fds[0], read_buffer, kTestDataSize);
  EXPECT_EQ(bytes_read, kTestDataSize);
  EXPECT_STREQ(read_buffer, kTestData);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, DataWrittenToPipeCreatedByPipe2CanBeRead) {
  int pipe_fds[2];
  int arbitrary_valid_flag = O_CLOEXEC;

  ASSERT_EQ(pipe2(pipe_fds, arbitrary_valid_flag), 0);

  char read_buffer[kTestDataSize];

  ssize_t bytes_written = write(pipe_fds[1], kTestData, kTestDataSize);
  EXPECT_EQ(bytes_written, kTestDataSize);

  ssize_t bytes_read = read(pipe_fds[0], read_buffer, kTestDataSize);
  EXPECT_EQ(bytes_read, kTestDataSize);
  EXPECT_STREQ(read_buffer, kTestData);

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(PosixPipeTest, DISABLED_PipeReturnsEMFILEIfTooManyFileDescriptorsOpen) {
  // TODO: b/412648662 - If/when getrlimit() and setrimit() are added to the
  // hermetic build, use them to drop the maximum value that the system can
  // assign to new file descriptors for this process to just above the current
  // usage in order to easily force this scenario. An equivalent test should be
  // added for pipe2(), as well.
  FAIL() << "This test requires the ability to manipulate process resource "
         << "limits (rlimit) to reliably trigger EMFILE, which is not "
         << "currently available in this test environment.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
