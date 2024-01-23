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

// The basic Sunny Day test is a subset of other Sunny Day tests, so it is not
// repeated here.

#if SB_API_VERSION >= 16

#if defined(_WIN32)
#include <errno.h>
#include <io.h>
#include <winsock2.h>
#else
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <utility>
#include "starboard/common/socket.h"
#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketBindTest : public ::testing::TestWithParam<int> {
 public:
  int GetAddressType() { return GetParam(); }
  int GetSocketDomain() { return GetParam() == 0 ? AF_INET : AF_INET6; }
};

int PosixSocketSetReuseAddress(int socket_fd, bool reuse) {
  // Set reuse address to true
  const int on = reuse ? 1 : 0;
  int return_value =
      setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  if (return_value != 0) {
    PosixSocketClose(socket_fd);
    return -1;
  }
  return 0;
}

int PosixSocketBind(int socket_fd, sockaddr* paddr, int addr_size) {
  if (socket_fd < 0 || paddr == NULL ||
      bind(socket_fd, paddr, addr_size) != 0) {
    return -1;
  }

  // Set reuse address to true
  PosixSocketSetReuseAddress(socket_fd, true);
  return 0;
}

TEST_P(PosixSocketBindTest, RainyDayNullSocket) {
  int port = GetPortNumberForTests();
  sockaddr_in address = {};
  address.sin_family = GetSocketDomain();
  int invalid_socket_fd = -1;
  EXPECT_FALSE(::bind(invalid_socket_fd, reinterpret_cast<sockaddr*>(&address),
                      sizeof(sockaddr_in)) == 0);
}

TEST_P(PosixSocketBindTest, RainyDayNullAddress) {
  int socket_domain, socket_type, socket_protocol;
  socket_domain = GetSocketDomain();
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  int socket_fd =
      PosixSocketCreate(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with a NULL address should fail.
  EXPECT_FALSE(::bind(socket_fd, NULL, 0));

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  sockaddr_in address = {};
  address.sin_family = GetSocketDomain();
  address.sin_port = GetPortNumberForTests();

  EXPECT_TRUE(::bind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                     sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

TEST_P(PosixSocketBindTest, RainyDayNullNull) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(::bind(invalid_socket_fd, NULL, 0));
}

#if SB_HAS(IPV6)
TEST_P(PosixSocketBindTest, RainyDayWrongAddressType) {
  int socket_domain, socket_type, socket_protocol;
  socket_domain = GetSocketDomain();
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  int socket_fd =
      PosixSocketCreate(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with the wrong address type should fail.
  sockaddr_in client_address = {};
  client_address.sin_family = GetSocketDomain();
  client_address.sin_port = GetPortNumberForTests();
  EXPECT_FALSE(::bind(socket_fd, reinterpret_cast<sockaddr*>(&client_address),
                      sizeof(sockaddr_in)) == 0);

  // Even though that failed, binding the same socket now with the server
  // address type should work.
  sockaddr_in server_address = {};
  server_address.sin_family = GetSocketDomain();
  server_address.sin_port = GetPortNumberForTests();
  EXPECT_TRUE(::bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                     sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}
#endif

TEST_P(PosixSocketBindTest, RainyDayBadInterface) {
  int socket_domain, socket_type, socket_protocol;
  socket_domain = GetSocketDomain();
  socket_type = SOCK_STREAM;
  socket_protocol = IPPROTO_TCP;

  int socket_fd =
      PosixSocketCreate(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with an interface that doesn't exist on this device should fail
  sockaddr_in server_address = {};
  server_address.sin_family = GetSocketDomain();
  // 250.x.y.z is reserved IP address
  server_address.sin_addr.s_addr = (((((250 << 8) | 43) << 8) | 244) << 8) | 18;
  EXPECT_FALSE(bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                    sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

TEST_P(PosixSocketBindTest, SunnyDayLocalInterface) {
  sockaddr address = {};

#if defined(_WIN32)
  char hostname[80];
  ASSERT_TRUE(gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR);
  struct hostent* phe = gethostbyname(hostname);
  ASSERT_TRUE(phe != 0);
  for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
    memcpy(address.sa_data, phe->h_addr_list[i], sizeof(struct in_addr));
    if (address.sa_family != AF_INET) {
      // IPv4 addresses only.
      continue;
    }
    break;
  }
#else
  struct ifaddrs* interface_addrs = NULL;
  ASSERT_TRUE(getifaddrs(&interface_addrs) == 0);
  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }
    memcpy(&address, interface->ifa_addr, sizeof(sockaddr));
    if (address.sa_family != AF_INET) {
      // IPv4 addresses only.
      continue;
    }
    break;
  }
#endif

  int socket_domain = GetSocketDomain();
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;

  int socket_fd =
      PosixSocketCreate(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);
  EXPECT_TRUE(PosixSocketBind(socket_fd, &address, sizeof(sockaddr) == 0));
  EXPECT_TRUE(PosixSocketClose(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
