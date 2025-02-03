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

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <tuple>
#include <utility>

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

class PosixSocketResolveTest
    : public ::testing::TestWithParam<std::tuple<int, std::pair<int, int>>> {
 public:
  int GetAddressFamily() {
    auto param = GetParam();
    return std::get<0>(param);
  }
  int GetSocketType() {
    auto param = GetParam();
    return std::get<1>(param).first;
  }
  int GetProtocol() {
    auto param = GetParam();
    return std::get<1>(param).second;
  }
};

// A random host name to use to test DNS resolution.
const char kTestHostName[] = "www.example.com";
const char kLocalhost[] = "localhost";

TEST_F(PosixSocketResolveTest, SunnyDay) {
  struct addrinfo hints = {0};
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  int address_count = 0;
  struct sockaddr_in* ai_addr = nullptr;

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
    if (ai_addr == nullptr && i->ai_addr != nullptr) {
      ai_addr = reinterpret_cast<sockaddr_in*>(i->ai_addr);
      break;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_NE(nullptr, ai_addr);

  EXPECT_TRUE(ai_addr->sin_family == AF_INET ||
              ai_addr->sin_family == AF_INET6);

  freeaddrinfo(ai);
}

#if SB_API_VERSION >= 16
TEST_P(PosixSocketResolveTest, SunnyDayFiltered) {
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    if (GetAddressFamily() != AF_UNSPEC) {
      EXPECT_EQ(i->ai_addr->sa_family, GetAddressFamily());
    } else {
      EXPECT_TRUE(i->ai_addr->sa_family == AF_INET ||
                  i->ai_addr->sa_family == AF_INET6);
    }
    if (GetSocketType() != 0) {
      EXPECT_EQ(i->ai_socktype, GetSocketType());
    }
    if (GetProtocol() != 0) {
      EXPECT_EQ(i->ai_protocol, GetProtocol());
    }
  }

  freeaddrinfo(ai);
}

TEST_P(PosixSocketResolveTest, SunnyDayFlags) {
  struct addrinfo hints = {0};
  int flags_to_test[] = {
  // Non-modular builds use native libc getaddrinfo.
#if defined(SB_MODULAR_BUILD)
      // And bionic does not support these flags.
      AI_V4MAPPED, AI_NUMERICHOST, AI_NUMERICSERV,
#endif
      AI_PASSIVE,  AI_CANONNAME,   AI_ADDRCONFIG,
  };
  for (auto flag : flags_to_test) {
    hints.ai_family = GetAddressFamily();
    hints.ai_socktype = GetSocketType();
    hints.ai_protocol = GetProtocol();
    hints.ai_flags = flag;
    struct addrinfo* ai = nullptr;

    int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
    EXPECT_EQ(result, 0);
    ASSERT_NE(nullptr, ai);

    for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
      EXPECT_EQ(i->ai_flags, hints.ai_flags);
      if (GetAddressFamily() != AF_UNSPEC) {
        EXPECT_EQ(i->ai_addr->sa_family, GetAddressFamily());
      } else {
        EXPECT_TRUE(i->ai_addr->sa_family == AF_INET ||
                    i->ai_addr->sa_family == AF_INET6);
      }
      if (GetSocketType() != 0) {
        EXPECT_EQ(i->ai_socktype, GetSocketType());
      }
      if (GetProtocol() != 0) {
        EXPECT_EQ(i->ai_protocol, GetProtocol());
      }
    }

    freeaddrinfo(ai);
  }
}
#endif  // SB_API_VERSION >= 16

TEST_P(PosixSocketResolveTest, Localhost) {
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kLocalhost, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  int address_count = 0;
  struct sockaddr_in* ai_addr = nullptr;
  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
    if (ai_addr == nullptr && i->ai_addr != nullptr) {
      ai_addr = reinterpret_cast<sockaddr_in*>(i->ai_addr);
      break;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_NE(nullptr, ai_addr);

  EXPECT_TRUE(ai_addr->sin_family == AF_INET ||
              ai_addr->sin_family == AF_INET6);
  if (GetAddressFamily() != AF_UNSPEC) {
    EXPECT_EQ(ai_addr->sin_family, GetAddressFamily());
  }

  freeaddrinfo(ai);
}

TEST_F(PosixSocketResolveTest, RainyDayNullHostname) {
  struct addrinfo hints = {0};
  struct addrinfo* ai = nullptr;

  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;

  EXPECT_FALSE(getaddrinfo(nullptr, nullptr, &hints, &ai) == 0);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_CASE_P(
    PosixSocketHints,
    PosixSocketResolveTest,
    ::testing::Combine(::testing::Values(AF_UNSPEC, AF_INET, AF_INET6),
                       ::testing::Values(std::make_pair(0, 0),
                                         std::make_pair(0, IPPROTO_UDP),
                                         std::make_pair(0, IPPROTO_TCP),
                                         std::make_pair(SOCK_STREAM, 0),
                                         std::make_pair(SOCK_DGRAM, 0))),
    GetPosixSocketHintsName);
#else
INSTANTIATE_TEST_CASE_P(
    PosixSocketHints,
    PosixSocketResolveTest,
    ::testing::Combine(::testing::Values(AF_UNSPEC, AF_INET),
                       ::testing::Values(std::make_pair(0, 0),
                                         std::make_pair(0, IPPROTO_UDP),
                                         std::make_pair(0, IPPROTO_TCP),
                                         std::make_pair(SOCK_STREAM, 0),
                                         std::make_pair(SOCK_DGRAM, 0))),
    GetPosixSocketHintsName);
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
