// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// SendTo is largely tested with ReceiveFrom, so look there for more involved
// tests.

#if SB_API_VERSION >= 16

#include <utility>

#if defined(_WIN32)
#include <errno.h>
#include <io.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "starboard/common/socket.h"
#include "starboard/common/time.h"
#include "starboard/memory.h"
#include "starboard/nplb/socket_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketSendtoTest : public ::testing::TestWithParam<int> {
 public:
};

class PosixPairSocketSendToTest
    : public ::testing::TestWithParam<std::pair<int, int> > {
 public:
  int GetServerAddressType() { return GetParam().first; }
  int GetClientAddressType() { return GetParam().second; }
};

int PosixSocketTcpSend(int socket_fd, const char* data, int data_size) {
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif

  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  ssize_t bytes_written = send(socket_fd, data, data_size, kSendFlags);
  if (bytes_written >= 0) {
    errno = kSbSocketOk;
    return bytes_written;
  }
  return -1;
}  // PosixSocketTcpSend()

int PosixSocketUdpSendTo(int socket_fd,
                         const char* data,
                         int data_size,
                         sockaddr* destination) {
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif

  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  sockaddr* sockaddr = NULL;

  ssize_t bytes_written = sendto(socket_fd, data, data_size, kSendFlags,
                                 destination, sizeof(sockaddr));

  if (bytes_written >= 0) {
    errno = kSbSocketOk;
    return bytes_written;
  }
  return -1;
}  // PosixSocketUdpSendTo()

// Thread entry point to continuously write to a socket that is expected to
// be closed on another thread.
struct trio_socket_fd {
  int* listen_socket_fd_ptr;
  int* server_socket_fd_ptr;
  int* client_socket_fd_ptr;
};

void* PosixSocketSendToServerSocketEntryPoint(void* trio_as_void_ptr) {
  // The contents of this buffer are inconsequential.
  struct trio_socket_fd* trio_ptr =
      reinterpret_cast<struct trio_socket_fd*>(trio_as_void_ptr);
  const size_t kBufSize = 1024;
  char* send_buf = new char[kBufSize];
  memset(send_buf, 0, kBufSize);

  // Continue sending to the socket until it fails to send. It's expected that
  // SbSocketSendTo will fail when the server socket closes, but the application
  // should not terminate.
  int64_t start = CurrentMonotonicTime();
  int64_t now = start;
  int64_t kTimeout = 1'000'000;  // 1 second
  int result = 0;
  while (result >= 0 && (now - start < kTimeout)) {
    result = PosixSocketTcpSend(*(trio_ptr->server_socket_fd_ptr), send_buf,
                                kBufSize);
    now = CurrentMonotonicTime();
  }

  delete[] send_buf;
  return NULL;
}

TEST(PosixSocketSendtoTest, RainyDayInvalidSocket) {
  char buf[16];
  int invalid_socket_fd = -1;
  int result = PosixSocketTcpSend(invalid_socket_fd, buf, sizeof(buf));
  EXPECT_EQ(-1, result);
}

TEST(PosixSocketSendtoTest, RainyDayUnconnectedSocket) {
  int socket_fd = PosixSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd >= 0);

  char buf[16];
  int result = PosixSocketTcpSend(socket_fd, buf, sizeof(buf));
  EXPECT_EQ(-1, result);

  EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE);

  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

TEST_P(PosixPairSocketSendToTest, RainyDaySendToClosedSocket) {
  int listen_socket_fd, server_socket_fd, client_socket_fd;
  struct trio_socket_fd trio_as_void_ptr = {0};

  int status = PosixSocketCreateAndConnect(
      GetServerAddressType(), GetClientAddressType(), GetPortNumberForTests(),
      kSocketTimeout, &listen_socket_fd, &server_socket_fd, &client_socket_fd);
  trio_as_void_ptr.client_socket_fd_ptr = &client_socket_fd;
  trio_as_void_ptr.server_socket_fd_ptr = &server_socket_fd;
  trio_as_void_ptr.listen_socket_fd_ptr = &listen_socket_fd;
  ASSERT_TRUE(status == 0);

  EXPECT_NE(client_socket_fd, -1);
  EXPECT_NE(server_socket_fd, -1);
  EXPECT_NE(listen_socket_fd, -1);

  // We don't need the listen socket, so close it.
  EXPECT_TRUE(PosixSocketClose(listen_socket_fd));
  listen_socket_fd = -1;

  // Start a thread to write to the client socket.
  const bool kJoinable = true;
  SbThread send_thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, kJoinable,
                     "SendToTest", PosixSocketSendToServerSocketEntryPoint,
                     static_cast<void*>(&trio_as_void_ptr));

  // Close the client, which should cause writes to the server socket to fail.
  EXPECT_TRUE(PosixSocketClose(client_socket_fd));

  // Wait for the thread to exit and check the last socket error.
  void* thread_result;
  EXPECT_TRUE(SbThreadJoin(send_thread, &thread_result));

  EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE);

  // Clean up the server socket.
  EXPECT_TRUE(PosixSocketClose(server_socket_fd));
}

// Tests the expectation that writing to a socket that is never drained
// will result in that socket becoming full and thus will return a
// kSbSocketPending status, which indicates that it is blocked.
TEST_P(PosixPairSocketSendToTest, RainyDaySendToSocketUntilBlocking) {
  static const int kChunkSize = 1024;
  // 1GB limit for sending data.
  static const uint64_t kMaxTransferLimit = 1024 * 1024 * 1024;

  int listen_socket_fd, server_socket_fd, client_socket_fd;

  int status = PosixSocketCreateAndConnect(
      GetServerAddressType(), GetClientAddressType(), GetPortNumberForTests(),
      kSocketTimeout, &listen_socket_fd, &server_socket_fd, &client_socket_fd);
  EXPECT_TRUE(status == 0);
  if (status != 0) {
    PosixSocketClose(listen_socket_fd);
    PosixSocketClose(server_socket_fd);
    PosixSocketClose(client_socket_fd);
  }

  // Push data into socket until it dies.
  uint64_t num_bytes = 0;
  while (num_bytes < kMaxTransferLimit) {
    char buff[kChunkSize] = {};
    int result = PosixSocketTcpSend(client_socket_fd, buff, sizeof(buff));

    if (result < 0) {
      // If we didn't get a socket, it should be pending.
      EXPECT_TRUE(errno == EINPROGRESS || errno == EAGAIN
#if EWOULDBLOCK != EAGAIN
                  || errno == EWOULDBLOCK
#endif
      );
    }

    if (result == 0) {  // Connection dropped unexpectedly.
      EXPECT_TRUE(false) << "Connection unexpectedly dropped.";
    }

    num_bytes += static_cast<uint64_t>(result);
  }
  EXPECT_TRUE(false) << "Max transfer rate reached.";
}

// Tests the expectation that killing a connection will cause the other
// connected socket to fail to write. For sockets without socket connection
// support this will show up as a generic error. Otherwise this will show
// up as a connection reset error.
TEST_P(PosixPairSocketSendToTest, RainyDaySendToSocketConnectionReset) {
  static const int kChunkSize = 1024;

  int listen_socket_fd, server_socket_fd, client_socket_fd;

  int status = PosixSocketCreateAndConnect(
      GetServerAddressType(), GetClientAddressType(), GetPortNumberForTests(),
      kSocketTimeout, &listen_socket_fd, &server_socket_fd, &client_socket_fd);
  EXPECT_TRUE(status == 0);
  if (status != 0) {
    PosixSocketClose(listen_socket_fd);
    PosixSocketClose(server_socket_fd);
    PosixSocketClose(client_socket_fd);
  }

  // Kills the server, the client socket will have it's connection reset during
  // one of the subsequent writes.
  PosixSocketClose(server_socket_fd);
  server_socket_fd = -1;

  // Expect that after some retries the client socket will return that the
  // connection will reset.
  int kNumRetries = 1000;
  for (int i = 0; i < kNumRetries; ++i) {
    char buff[kChunkSize] = {};
    SbThreadSleep(1000);
    int result = PosixSocketTcpSend(client_socket_fd, buff, sizeof(buff));

    if (result < 0) {
      EXPECT_TRUE(errno == ECONNRESET || errno == ENETRESET || errno == EPIPE);
      return;
    }

    if (result == 0) {
      return;  // Other way in which the connection was reset.
    }
  }
  ASSERT_TRUE(false) << "Connection was not dropped after " << kNumRetries
                     << " tries.";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
