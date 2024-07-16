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

#include "starboard/configuration.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

int CreateMulticastSocket(const struct ip_mreq& address) {
  int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EXPECT_NE(-1, socket_fd);

  int reuse = 1;
  EXPECT_NE(-1, setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                           sizeof(reuse)));

  // It will choose a port for me.
  struct sockaddr_in bind_address;
  memset(&bind_address, 0, sizeof(bind_address));
  bind_address.sin_family = AF_INET;
  bind_address.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_address.sin_port = htons(0);

  EXPECT_NE(-1, bind(socket_fd, (struct sockaddr*)&bind_address,
                     sizeof(bind_address)));

  EXPECT_NE(-1, setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &address,
                           sizeof(address)));
  return socket_fd;
}

int CreateSendSocket() {
  int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EXPECT_NE(-1, socket_fd);

  int reuse = 1;
  EXPECT_NE(-1, setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                           sizeof(reuse)));
  return socket_fd;
}

TEST(PosixSocketJoinMulticastGroupTest, SunnyDay) {
  // "224.0.2.0" is an unassigned ad-hoc multicast address. Hopefully no one is
  // spamming it on the local network.
  // http://www.iana.org/assignments/multicast-addresses/multicast-addresses.xhtml
  struct ip_mreq address;
// Same as doing inet_addr("224.0.2.0");, however inet_addr is not supported
// yet in Starboard 16.
// TODO: we should support inet_addr for better handling endianness across
// different systems.
#if SB_IS_BIG_ENDIAN
  address.imr_multiaddr.s_addr = (224 << 24) | (0 << 16) | (2 << 8) | 0;
#else
  address.imr_multiaddr.s_addr = (0 << 24) | (2 << 16) | (0 << 8) | 224;
#endif
  address.imr_interface.s_addr =
      htonl(INADDR_ANY);  // Use the default network interface

  int receive_socket = CreateMulticastSocket(address);
  int send_socket = CreateSendSocket();

  // Get the bound port.
  struct sockaddr_in local_address;
  socklen_t local_address_len = sizeof(local_address);
  EXPECT_NE(-1, getsockname(receive_socket, (struct sockaddr*)&local_address,
                            &local_address_len));

  struct sockaddr_in send_address;
  memset(&send_address, 0, sizeof(send_address));
  send_address.sin_family = AF_INET;

#if SB_IS_BIG_ENDIAN
  send_address.sin_addr.s_addr = (224 << 24) | (0 << 16) | (2 << 8) | 0;
#else
  send_address.sin_addr.s_addr = (0 << 24) | (2 << 16) | (0 << 8) | 224;
#endif
  send_address.sin_port = local_address.sin_port;

  const char kBuf[] = "01234567890123456789";
  char buf[sizeof(kBuf)] = {0};
  while (true) {
    ssize_t sent =
        sendto(send_socket, kBuf, sizeof(kBuf), 0,
               (struct sockaddr*)&send_address, sizeof(send_address));
    if (sent < 0) {
      if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }

      // Clean up the sockets.
      EXPECT_NE(-1, close(send_socket));
      EXPECT_NE(-1, close(receive_socket));
      FAIL() << "Failed to send multicast packet: " << errno;
      return;
    }
    EXPECT_EQ(sizeof(kBuf), sent);
    break;
  }

  struct sockaddr_in receive_address;
  socklen_t receive_address_len = sizeof(receive_address);
  int64_t stop_time = CurrentMonotonicTime() + 1'000'000LL;

  while (true) {
    // Breaks the case where the test will hang in a loop when
    // recvfrom always returns pending status.
    ASSERT_LE(CurrentMonotonicTime(), stop_time) << "Multicast timed out.";
    ssize_t received =
        recvfrom(receive_socket, buf, sizeof(buf), 0,
                 (struct sockaddr*)&receive_address, &receive_address_len);
    if (received < 0 &&
        (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK)) {
      usleep(1000);
      continue;
    }
    EXPECT_EQ(sizeof(kBuf), received);
    break;
  }

  for (size_t i = 0; i < sizeof(kBuf); ++i) {
    EXPECT_EQ(kBuf[i], buf[i]) << "position " << i;
  }

  EXPECT_NE(-1, close(send_socket));
  EXPECT_NE(-1, close(receive_socket));
}

TEST(PosixSocketJoinMulticastGroupTest, RainyDayInvalidSocket) {
  struct ip_mreq mreq;
#if SB_IS_BIG_ENDIAN
  mreq.imr_multiaddr.s_addr = (224 << 24) | (0 << 16) | (2 << 8) | 0;
#else
  mreq.imr_multiaddr.s_addr = (0 << 24) | (2 << 16) | (0 << 8) | 224;
#endif
  mreq.imr_interface.s_addr =
      htonl(INADDR_ANY);  // Use the default network interface

  EXPECT_EQ(-1,
            setsockopt(-1, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)));
  EXPECT_TRUE(errno == EBADF || errno == ECONNREFUSED);
}

TEST(PosixSocketJoinMulticastGroupTest, RainyDayInvalidAddress) {
  int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EXPECT_NE(-1, socket_fd);

  EXPECT_EQ(-1, setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, NULL,
                           sizeof(struct ip_mreq)));
  EXPECT_TRUE(errno == EBADF || errno == EFAULT);

  EXPECT_NE(-1, close(socket_fd));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
