// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "starboard/thread.h"

namespace starboard {
namespace nplb {
namespace {

void* PosixSocketSendToServerSocketEntryPoint(void* trio_as_void_ptr) {
  // The contents of this buffer are inconsequential.
  struct trio_socket_fd* trio_ptr =
      reinterpret_cast<struct trio_socket_fd*>(trio_as_void_ptr);
  const size_t kBufSize = 1024;
  char* send_buf = new char[kBufSize];
  memset(send_buf, 0, kBufSize);

  pthread_setname_np(pthread_self(), "SendToTest");

  // Continue sending to the socket until it fails to send. It's expected that
  // SbSocketSendTo will fail when the server socket closes, but the application
  // should not terminate.
  int64_t start = CurrentMonotonicTime();
  int64_t now = start;
  int64_t kTimeout = 1'000'000;  // 1 second
  int result = 0;
  while (result >= 0 && (now - start < kTimeout)) {
    result = sendto(*(trio_ptr->server_socket_fd_ptr), send_buf, kBufSize,
                    kSendFlags, NULL, 0);
    now = CurrentMonotonicTime();
  }

  delete[] send_buf;
  return NULL;
}

TEST(PosixSocketSendtoTest, RainyDayInvalidSocket) {
  char buf[16];
  int invalid_socket_fd = -1;

  ssize_t bytes_written =
      sendto(invalid_socket_fd, buf, sizeof(buf), kSendFlags, NULL, 0);
  EXPECT_FALSE(bytes_written >= 0);
}

TEST(PosixSocketSendtoTest, RainyDayUnconnectedSocket) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  char buf[16];
  ssize_t bytes_written =
      sendto(socket_fd, buf, sizeof(buf), kSendFlags, NULL, 0);
  EXPECT_FALSE(bytes_written >= 0);

  EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE ||
              errno == ENOTCONN);
  SB_DLOG(INFO) << "Failed to send, errno = " << strerror(errno);

  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketSendtoTest, RainyDaySendToClosedSocket) {
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(GetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);

  // We don't need the listen socket, so close it.
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  listen_socket_fd = -1;

  struct trio_socket_fd trio_as_void_ptr = {
      &listen_socket_fd, &client_socket_fd, &server_socket_fd};

  // Start a thread to write to the client socket.
  const bool kJoinable = true;
  pthread_t send_thread = 0;
  pthread_create(&send_thread, NULL, PosixSocketSendToServerSocketEntryPoint,
                 static_cast<void*>(&trio_as_void_ptr));

  // Close the client, which should cause writes to the server socket to
  EXPECT_TRUE(close(client_socket_fd) == 0);

  // Wait for the thread to exit and check the last socket error.
  void* thread_result;
  EXPECT_TRUE(pthread_join(send_thread, &thread_result) == 0);

  EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE ||
              errno == ENOTCONN ||     // errno on Windows
              errno == EINPROGRESS ||  // errno on Evergreen
              errno == ENETUNREACH     // errno on raspi
  );
  SB_DLOG(INFO) << "Failed to send, errno = " << strerror(errno);

  // Clean up the server socket.
  EXPECT_TRUE(close(server_socket_fd) == 0);
}

// Tests the expectation that writing to a socket that is never drained
// will result in that socket becoming full and thus will return a
// kSbSocketPending status, which indicates that it is blocked.
TEST(PosixSocketSendtoTest, RainyDaySendToSocketUntilBlocking) {
  static const int kChunkSize = 1024;
  // 1GB limit for sending data.
  static const uint64_t kMaxTransferLimit = 1024 * 1024 * 1024;

  int result = -1;
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(GetPortNumberForTests()), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);

  // set socket non-blocking
  fcntl(client_socket_fd, F_SETFL, O_NONBLOCK);

  // Push data into socket until it dies.
  uint64_t num_bytes = 0;
  result = 0;
  while (num_bytes < kMaxTransferLimit) {
    char buff[kChunkSize] = {};
    result = sendto(client_socket_fd, buff, sizeof(buff), kSendFlags, NULL, 0);

    if (result < 0) {
      // If we didn't get a socket, it should be pending.
      EXPECT_TRUE(errno == EINPROGRESS || errno == EAGAIN ||
                  errno == EWOULDBLOCK);
      SB_DLOG(INFO) << "Failed to send, errno = " << strerror(errno);
      break;
    }

    if (result == 0) {  // Connection dropped unexpectedly.
      EXPECT_TRUE(false) << "Connection unexpectedly dropped.";
    }

    num_bytes += static_cast<uint64_t>(result);
  }
  if (num_bytes >= kMaxTransferLimit) {
    EXPECT_TRUE(false) << "Max transfer rate reached.";
  }
  EXPECT_TRUE(close(server_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
}

// Tests the expectation that killing a connection will cause the other
// connected socket to fail to write. For sockets without socket connection
// support this will show up as a generic error. Otherwise this will show
// up as a connection reset error.
TEST(PosixSocketSendtoTest, RainyDaySendToSocketConnectionReset) {
  static const int kChunkSize = 1024;
  int result = -1;

  // create listen socket, bind and listen on <port>
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  PosixSocketCreateAndConnect(AF_INET, AF_INET, htons(GetPortNumberForTests()),
                              kSocketTimeout, &listen_socket_fd,
                              &client_socket_fd, &server_socket_fd);

  // Kills the server, the client socket will have it's connection reset during
  // one of the subsequent writes.
  EXPECT_TRUE(close(server_socket_fd) == 0);
  server_socket_fd = -1;

  // Expect that after some retries the client socket will return that the
  // connection will reset.
  int kNumRetries = 1000;
  for (int i = 0; i < kNumRetries; ++i) {
    char buff[kChunkSize] = {};
    usleep(1000);
    result = sendto(client_socket_fd, buff, sizeof(buff), kSendFlags, NULL, 0);

    if (result < 0) {
      EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE ||
                  errno == ECONNABORTED);
      SB_DLOG(INFO) << "Failed to send, errno = " << strerror(errno);
      break;
    }

    if (result == 0) {
      SB_DLOG(INFO) << "Other way in which the connection was reset.";
      break;
    }
  }

  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
