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

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

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
      int bytes_sent = sendto(send_socket_fd, send_data + send_total,
                              size - send_total, kSendFlags, NULL, 0);
      if (bytes_sent < 0) {
        if (errno != EINPROGRESS) {
          return -1;
        }
        bytes_sent = 0;
      }

      send_total += bytes_sent;
    }

    int bytes_received = recvfrom(receive_socket_fd, out_data + receive_total,
                                  size - receive_total, 0, NULL, 0);

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

TEST(PosixSocketRecvfromTest, SunnyDay) {
  const int kBufSize = 256 * 1024;
  const int kSockBufSize = kBufSize / 8;
  int listen_socket_fd = -1, client_socket_fd = -1, server_socket_fd = -1;
  int result = PosixSocketCreateAndConnect(
      AF_INET, AF_INET, htons(PosixGetPortNumberForTests()), kSocketTimeout,
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
