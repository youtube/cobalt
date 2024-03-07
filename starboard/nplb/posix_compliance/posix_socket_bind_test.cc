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
#if SB_API_VERSION >= 16

#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/nplb/socket_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

// Helper functions
int PosixGetLocalAddressiIPv4(sockaddr* address_ptr) {
  int result = -1;
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }
  /* Walk through linked list, maintaining head pointer so we
              can free list later. */
  for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    /* For an AF_INET* interface address, display the address. */
    if (ifa->ifa_addr->sa_family == AF_INET) {
      memcpy(address_ptr, ifa->ifa_addr, sizeof(struct sockaddr_in));
      result = 0;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

#if SB_HAS(IPV6)
int PosixGetLocalAddressiIPv6(sockaddr_in6* address_ptr) {
  int result = -1;
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return -1;
  }
  /* Walk through linked list, maintaining head pointer so we
              can free list later. */
  for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    /* For an AF_INET* interface address, display the address. */
    if (ifa->ifa_addr->sa_family == AF_INET6) {
      memcpy(address_ptr, ifa->ifa_addr, sizeof(struct sockaddr_in6));
      result = 0;
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}
#endif

namespace {

TEST(PosixSocketBindTest, RainyDayNullSocket) {
  int port = GetPortNumberForTests();
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  int invalid_socket_fd = -1;
  EXPECT_FALSE(bind(invalid_socket_fd, reinterpret_cast<sockaddr*>(&address),
                    sizeof(sockaddr_in)) == 0);
}

TEST(PosixSocketBindTest, RainyDayNullAddress) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with a NULL address should fail.
  EXPECT_FALSE(bind(socket_fd, NULL, 0) == 0);

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = GetPortNumberForTests();

  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketBindTest, RainyDayNullNull) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(bind(invalid_socket_fd, NULL, 0) == 0);
}

#if SB_HAS(IPV6)
TEST(PosixSocketBindTest, RainyDayWrongAddressType) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with the wrong address type should fail.
  sockaddr_in client_address = {};
  client_address.sin_family = AF_INET6;
  client_address.sin_port = GetPortNumberForTests();
  EXPECT_FALSE(bind(socket_fd, reinterpret_cast<sockaddr*>(&client_address),
                    sizeof(sockaddr_in)) == 0);

  // Even though that failed, binding the same socket now with the server
  // address type should work.
  sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_port = GetPortNumberForTests();
  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}
#endif

TEST(PosixSocketBindTest, RainyDayBadInterface) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with an interface that doesn't exist on this device should fail
  sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  // 250.x.y.z is reserved IP address
  server_address.sin_addr.s_addr = (((((250 << 8) | 43) << 8) | 244) << 8) | 18;
  EXPECT_FALSE(bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                    sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketBindTest, SunnyDayLocalInterface) {
  sockaddr_in6 address = {};
#if SB_HAS(IPV6)
  EXPECT_TRUE(PosixGetLocalAddressiIPv4(
                  reinterpret_cast<struct sockaddr*>(&address)) == 0 ||
              PosixGetLocalAddressiIPv6(&address) == 0);
#else
  EXPECT_TRUE(
      PosixGetLocalAddressiIPv4(reinterpret_cast<sockaddr*>(&address)) == 0);
#endif

  int socket_domain = AF_INET;
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;

  int socket_fd = socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);
  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address),
                   sizeof(struct sockaddr)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= 16
