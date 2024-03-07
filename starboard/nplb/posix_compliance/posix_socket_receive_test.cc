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

#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

int PosixSocketCreateAndConnect(int server_domain,
                                int client_domain,
                                int port,
                                int64_t timeout,
                                int* listen_socket_fd,
                                int* client_socket_fd,
                                int* server_socket_fd) {
  int result = -1;
  // create listen socket, bind and listen on <port>
  *listen_socket_fd = socket(server_domain, SOCK_STREAM, IPPROTO_TCP);
  if (*listen_socket_fd < 0) {
    return -1;
  }
  // set socket reuseable
  const int on = 1;
  result =
      setsockopt(*listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }
  // bind socket with local address
  struct sockaddr_in address = {};
  result =
      PosixGetLocalAddressiIPv4(reinterpret_cast<struct sockaddr*>(&address));
  address.sin_port = port;
  address.sin_family = AF_INET;
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }
  result = bind(*listen_socket_fd, reinterpret_cast<struct sockaddr*>(&address),
                sizeof(struct sockaddr_in));
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }
  result = listen(*listen_socket_fd, kMaxConn);
  if (result != 0) {
    close(*listen_socket_fd);
    return -1;
  }

  // create client socket and connect to localhost:<port>
  *client_socket_fd = socket(client_domain, SOCK_STREAM, IPPROTO_TCP);
  if (*client_socket_fd < 0) {
    close(*listen_socket_fd);
    return -1;
  }
  result =
      setsockopt(*client_socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (result != 0) {
    close(*listen_socket_fd);
    close(*client_socket_fd);
    return -1;
  }
  result =
      connect(*client_socket_fd, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(struct sockaddr_in));
  if (result != 0) {
    close(*listen_socket_fd);
    close(*client_socket_fd);
    return -1;
  }

  int64_t start = CurrentMonotonicTime();
  while ((CurrentMonotonicTime() - start < timeout)) {
    *server_socket_fd = accept(*listen_socket_fd, NULL, NULL);
    if (*server_socket_fd > 0) {
      return 0;
    }

    // If we didn't get a socket, it should be pending.
#if EWOULDBLOCK != EAGAIN
    if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK)
#else
    if (errno == EINPROGRESS || errno == EAGAIN)
#endif
    {
      // Just being polite.
      SbThreadYield();
    }
  }

  // timeout expired
  close(*listen_socket_fd);
  close(*client_socket_fd);
  return -1;
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
      int bytes_sent = send(send_socket_fd, send_data + send_total,
                            size - send_total, kSendFlags);
      if (bytes_sent < 0) {
        if (errno != EINPROGRESS) {
          return -1;
        }
        bytes_sent = 0;
      }

      send_total += bytes_sent;
    }

    int bytes_received = recv(receive_socket_fd, out_data + receive_total,
                              size - receive_total, 0);

    if (bytes_received < 0) {
      if (errno != EINPROGRESS) {
        return -1;
      }
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

namespace {

TEST(PosixSocketReceiveTest, SunnyDay) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, GetPortNumberForTests(), kSocketTimeout,
      &listen_socket_fd, &client_socket_fd, &server_socket_fd);
  ASSERT_TRUE(result == 0);

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
  int transferred = 0;
  transferred = Transfer(client_socket_fd, receive_buf, server_socket_fd,
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

  EXPECT_TRUE(close(server_socket_fd) == 0);
  EXPECT_TRUE(close(client_socket_fd) == 0);
  EXPECT_TRUE(close(listen_socket_fd) == 0);
  delete[] send_buf;
  delete[] receive_buf;
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
