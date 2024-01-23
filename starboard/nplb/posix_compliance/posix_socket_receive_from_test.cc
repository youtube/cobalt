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
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketReceiveFromTest : public ::testing::TestWithParam<int> {
 public:
};

class PosixPairSocketReceiveFromTest
    : public ::testing::TestWithParam<std::pair<int, int> > {
 public:
  int GetServerAddressType() { return GetParam().first; }
  int GetServerSocketDomain() {
    return GetParam().first == 0 ? AF_INET : AF_INET6;
  }
  int GetClientAddressType() { return GetParam().second; }
  int GetClientSocketDomain() {
    return GetParam().second == 0 ? AF_INET : AF_INET6;
  }
};

int PosixSocketTcpReceive(int socket_fd, char* out_data, int data_size) {
  const int kRecvFlags = 0;

  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  ssize_t bytes_read = recv(socket_fd, out_data, data_size, kRecvFlags);
  if (bytes_read >= 0) {
    return static_cast<int>(bytes_read);
  }

  return -1;
}

int PosixSocketUdpReceiveFrom(int socket_fd,
                              char* out_data,
                              int data_size,
                              sockaddr* address) {
  const int kRecvFlags = 0;

  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }
  socklen_t sockaddr_length = sizeof(sockaddr);
  size_t bytes_read = ::recvfrom(socket_fd, out_data, data_size, kRecvFlags,
                                 address, &sockaddr_length);

  if (bytes_read >= 0) {
    return static_cast<int>(bytes_read);
  }

  return -1;
}

int PosixSocketCreateAndConnect(int server_address_type,
                                int client_address_type,
                                int port,
                                int64_t timeout,
                                int* listen_socket_fd,
                                int* server_socket_fd,
                                int* client_socket_fd) {
  // Verify the listening socket.
  int domain = server_address_type == 0 ? AF_INET : AF_INET6;
  *listen_socket_fd =
      PosixCreateListeningTcpSocket(domain, SOCK_STREAM, IPPROTO_TCP, port);
  if (*listen_socket_fd < 0) {
    return -1;
  }

  // Verify the socket to connect to the listening socket.
  domain = client_address_type == 0 ? AF_INET : AF_INET6;
  *client_socket_fd =
      PosixCreateConnectingTcpSocket(domain, SOCK_STREAM, IPPROTO_TCP, port);
  if (*client_socket_fd < 0) {
    PosixSocketClose(*listen_socket_fd);
    *listen_socket_fd = -1;
    return -1;
  }

  // Spin until the accept happens (or we get impatient).
  int64_t start = CurrentMonotonicTime();
  *server_socket_fd = PosixSocketAcceptBySpinning(*listen_socket_fd, timeout);
  if (*server_socket_fd < 0) {
    EXPECT_TRUE(PosixSocketClose(*listen_socket_fd));
    *listen_socket_fd = -1;
    EXPECT_TRUE(PosixSocketClose(*client_socket_fd));
    *client_socket_fd = -1;
    return -1;
  }

  return 0;
}

// Transfers data between the two connected local sockets, spinning until |size|
// has been transferred, or an error occurs.
int Transfer(int receive_socket_fd,
             char* out_data,
             int send_socket_fd,
             const char* send_data,
             int size) {
  int send_total = 0;
  int receive_total = 0;

  while (receive_total < size) {
    if (send_total < size) {
      int bytes_sent = PosixSocketTcpSend(
          send_socket_fd, send_data + send_total, size - send_total);
      if (bytes_sent < 0) {
        bytes_sent = 0;
        return -1;
      }

      send_total += bytes_sent;
    }

    int bytes_received = PosixSocketTcpReceive(
        receive_socket_fd, out_data + receive_total, size - receive_total);

    if (bytes_received < 0) {
      bytes_received = 0;
    }

    receive_total += bytes_received;
  }

  return size;
}

int PosixSocketSetReceiveBufferSize(int socket_fd, int32_t size) {
  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  int result = setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF", size);
  if (result != 0) {
    return -1;
  }

  return 0;
}

int PosixSocketSetSendBufferSize(int socket_fd, int32_t size) {
  if (socket_fd < 0) {
    errno = EBADF;
    return -1;
  }

  int result = setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, "SO_SNDBUF", size);
  if (result != 0) {
    return -1;
  }
  return 0;
}

TEST(PosixSocketReceiveFromTest, RainyDayInvalidSocket) {
  char buf[16];
  int invalid_socket_fd = -1;
  int result;
  const int kRecvFlags = 0;
  struct sockaddr address = {0};
  socklen_t address_length = sizeof(sockaddr);

  EXPECT_FALSE(::recvfrom(invalid_socket_fd, buf, sizeof(buf), kRecvFlags,
                          &address, &address_length) >= 0);
}

TEST_P(PosixPairSocketReceiveFromTest, SunnyDay) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;

  int listen_socket_fd, server_socket_fd, client_socket_fd;

  int status = PosixSocketCreateAndConnect(
      GetServerAddressType(), GetClientAddressType(), GetPortNumberForTests(),
      kSocketTimeout, &listen_socket_fd, &server_socket_fd, &client_socket_fd);
  ASSERT_TRUE(server_socket_fd >= 0 && status == 0);

  // Let's set the buffers small to create partial reads and writes.
  PosixSocketSetReceiveBufferSize(client_socket_fd, kSockBufSize);
  PosixSocketSetReceiveBufferSize(server_socket_fd, kSockBufSize);
  PosixSocketSetSendBufferSize(client_socket_fd, kSockBufSize);
  PosixSocketSetSendBufferSize(server_socket_fd, kSockBufSize);

  // Create the buffers and fill the send buffer with a pattern, the receive
  // buffer with zeros.
  char* send_buf = new char[kBufSize];
  char* receive_buf = new char[kBufSize];
  for (int i = 0; i < kBufSize; ++i) {
    send_buf[i] = static_cast<char>(i);
    receive_buf[i] = 0;
  }

  // Send from server to client and verify the pattern.
  int transferred = Transfer(client_socket_fd, receive_buf, server_socket_fd,
                             send_buf, kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(send_buf[i], receive_buf[i]) << "Position " << i;
  }

  // Try the other way now, first clearing the target buffer again.
  for (int i = 0; i < kBufSize; ++i) {
    receive_buf[i] = 0;
  }
  transferred = Transfer(server_socket_fd, receive_buf, client_socket_fd,
                         send_buf, kBufSize);
  EXPECT_EQ(kBufSize, transferred);
  for (int i = 0; i < kBufSize; ++i) {
    EXPECT_EQ(send_buf[i], receive_buf[i]) << "Position " << i;
  }

  EXPECT_TRUE(PosixSocketClose(server_socket_fd));
  EXPECT_TRUE(PosixSocketClose(client_socket_fd));
  EXPECT_TRUE(PosixSocketClose(listen_socket_fd));
  delete[] send_buf;
  delete[] receive_buf;
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
