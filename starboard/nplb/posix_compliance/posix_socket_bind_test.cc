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

#include "starboard/common/string.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixSocketBindTest, RainyDayNullSocket) {
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
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketBindTest, RainyDayNullNull) {
  int invalid_socket_fd = -1;
  EXPECT_FALSE(bind(invalid_socket_fd, NULL, 0) == 0);
}

TEST(PosixSocketBindTest, RainyDayWrongAddressType) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with the wrong address type should fail.
  sockaddr_in client_address = {};
  client_address.sin_family = AF_INET6;
  client_address.sin_port = htons(PosixGetPortNumberForTests());
  EXPECT_FALSE(bind(socket_fd, reinterpret_cast<sockaddr*>(&client_address),
                    sizeof(sockaddr_in)) == 0);

  // Even though that failed, binding the same socket now with the server
  // address type should work.
  sockaddr_in server_address = {};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PosixGetPortNumberForTests());
  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

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
  EXPECT_TRUE(
      PosixGetLocalAddressIPv4(reinterpret_cast<sockaddr*>(&address)) == 0 ||
      PosixGetLocalAddressIPv6(reinterpret_cast<sockaddr*>(&address)) == 0);
  address.sin6_port = htons(PosixGetPortNumberForTests());

  int socket_domain = AF_INET;
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;

  int socket_fd = socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);

  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<struct sockaddr*>(&address),
                   sizeof(struct sockaddr)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST(PosixSocketBindTest, SunnyDayAnyAddr) {
  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(PosixGetPortNumberForTests());
  address.sin_addr.s_addr = INADDR_ANY;

  int socket_domain = AF_INET;
  int socket_type = SOCK_STREAM;
  int socket_protocol = IPPROTO_TCP;
  int socket_fd = socket(socket_domain, socket_type, socket_protocol);
  ASSERT_TRUE(socket_fd > 0);
  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

// Pair data input test
std::string GetPosixSocketAddressTypeFilterPairName(
    ::testing::TestParamInfo<std::pair<int, int>> info) {
  return FormatString("type_%d_filter_%d", info.param.first, info.param.second);
}

class PosixSocketBindPairFilterTest
    : public ::testing::TestWithParam<std::pair<int, int>> {
 public:
  int GetAddressType() { return GetParam().first; }
  int GetFilterType() { return GetParam().second; }
};

class PosixSocketBindPairCSTest
    : public ::testing::TestWithParam<std::pair<int, int>> {
 public:
  int GetServerAddressType() { return GetParam().first; }
  int GetClientAddressType() { return GetParam().second; }
};

TEST_P(PosixSocketBindPairFilterTest, RainyDayNullSocketPair) {
  sockaddr_in address = {};
  address.sin_family = GetAddressType();
  address.sin_port = htons(PosixGetPortNumberForTests());

  int invalid_socket_fd = -1;

  EXPECT_FALSE(bind(invalid_socket_fd, reinterpret_cast<sockaddr*>(&address),
                    sizeof(sockaddr_in)) == 0);
}

TEST_P(PosixSocketBindPairFilterTest, RainyDayNullAddressPair) {
  return;
  int socket_fd = socket(GetAddressType(), SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with a NULL address should fail.
  EXPECT_FALSE(bind(socket_fd, NULL, 0) == 0);

  // Even though that failed, binding the same socket now with 0.0.0.0:2048
  // should work.
  sockaddr_in address = {};
  address.sin_family = GetAddressType();
  address.sin_port = htons(PosixGetPortNumberForTests());

  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST_P(PosixSocketBindPairFilterTest, RainyDayBadInterfacePair) {
  return;
  int socket_fd = socket(GetAddressType(), SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with an interface that doesn't exist on this device should fail, so
  // let's find an address of a well-known public website that we shouldn't be
  // able to bind to.
  const char* kTestHostName = "www.yahoo.com";

  struct addrinfo* ai = nullptr;
  struct addrinfo hints = {0};
  hints.ai_family = GetFilterType();
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;

  // Most likely success since it is a well known website
  int result = getaddrinfo(kTestHostName, nullptr, &hints, &ai);
  EXPECT_TRUE(result == 0);
  if (result < 0) {
    close(socket_fd);
    return;
  }

  int address_count = 0;
  for (struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
  }
  EXPECT_LT(0, address_count);

  // Extract the address out of the addrinfo structure
  struct sockaddr server_address = {};

  int index = 0;
  for (struct addrinfo* i = ai; i != nullptr; i = i->ai_next, ++index) {
    // Skip over any addresses we can't parse.
    if (i->ai_addr != NULL) {
      memcpy(&server_address, i->ai_addr, i->ai_addrlen);
      break;
    }
  }

  freeaddrinfo(ai);

  EXPECT_FALSE(bind(socket_fd, &server_address, sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

TEST_P(PosixSocketBindPairCSTest, RainyDayWrongAddressTypePair) {
  return;
  int socket_fd = socket(GetServerAddressType(), SOCK_STREAM, IPPROTO_TCP);
  ASSERT_TRUE(socket_fd > 0);

  // Binding with the wrong address type should fail.
  sockaddr_in client_address = {};
  client_address.sin_family = GetClientAddressType();
  client_address.sin_port = htons(PosixGetPortNumberForTests());
  EXPECT_FALSE(bind(socket_fd, reinterpret_cast<sockaddr*>(&client_address),
                    sizeof(sockaddr_in)) == 0);

  // Even though that failed, binding the same socket now with the server
  // address type should work.
  sockaddr_in server_address = {};
  server_address.sin_family = GetServerAddressType();
  server_address.sin_port = htons(PosixGetPortNumberForTests());
  EXPECT_TRUE(bind(socket_fd, reinterpret_cast<sockaddr*>(&server_address),
                   sizeof(sockaddr_in)) == 0);
  EXPECT_TRUE(close(socket_fd) == 0);
}

INSTANTIATE_TEST_SUITE_P(PosixSocketBindTest,
                         PosixSocketBindPairFilterTest,
                         ::testing::Values(std::make_pair(AF_INET, AF_INET),
                                           std::make_pair(AF_INET6, AF_INET6)),
                         GetPosixSocketAddressTypeFilterPairName);
INSTANTIATE_TEST_SUITE_P(PosixSocketBindTest,
                         PosixSocketBindPairCSTest,
                         ::testing::Values(std::make_pair(AF_INET, AF_INET6),
                                           std::make_pair(AF_INET6, AF_INET)),
                         GetPosixSocketAddressTypeFilterPairName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
