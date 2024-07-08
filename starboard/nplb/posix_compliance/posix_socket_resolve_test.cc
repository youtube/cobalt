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

#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"

namespace starboard {
namespace nplb {
namespace {

// A random host name to use to test DNS resolution.
const char kTestHostName[] = "www.example.com";
const char kLocalhost[] = "localhost";

TEST(PosixSocketResolveTest, SunnyDay) {
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
TEST(PosixSocketResolveTest, SunnyDaySocketType) {
  struct addrinfo hints = {0};
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  struct sockaddr_in* ai_addr = nullptr;

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    EXPECT_EQ(i->ai_socktype, SOCK_DGRAM);
  }

  freeaddrinfo(ai);
}

TEST(PosixSocketResolveTest, SunnyDayFamily) {
  struct addrinfo hints = {0};
  hints.ai_family = AF_INET6;
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    EXPECT_EQ(i->ai_addr->sa_family, AF_INET6);
  }

  freeaddrinfo(ai);
}

TEST(PosixSocketResolveTest, SunnyDayFlags) {
  struct addrinfo hints = {0};
  int flags_to_test[] = {
      AI_PASSIVE, AI_ADDRCONFIG, AI_PASSIVE, AI_CANONNAME, AI_V4MAPPED, AI_ALL,
  };
  for (auto flag : flags_to_test) {
    hints.ai_flags |= flag;
    struct addrinfo* ai = nullptr;

    int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
    EXPECT_EQ(result, 0);
    ASSERT_NE(nullptr, ai);

    for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
      EXPECT_EQ(i->ai_flags, hints.ai_flags);
    }

    freeaddrinfo(ai);
  }
}

TEST(PosixSocketResolveTest, SunnyDayProtocol) {
  struct addrinfo hints = {0};
  int protocol_to_test[] = {
      IPPROTO_TCP,
      IPPROTO_UDP,
  };
  for (auto protocol : protocol_to_test) {
    hints.ai_protocol = protocol;
    struct addrinfo* ai = nullptr;

    int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
    EXPECT_EQ(result, 0);
    ASSERT_NE(nullptr, ai);

    for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
      EXPECT_EQ(i->ai_protocol, hints.ai_protocol);
    }

    freeaddrinfo(ai);
  }
}
#endif  // SB_API_VERSION >= 16

TEST(PosixSocketResolveTest, Localhost) {
  struct addrinfo hints = {0};
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

  freeaddrinfo(ai);
}

TEST(PosixSocketResolveTest, RainyDayNullHostname) {
  struct addrinfo hints = {0};
  struct addrinfo* ai = nullptr;

  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_socktype = SOCK_STREAM;

  EXPECT_FALSE(getaddrinfo(nullptr, nullptr, &hints, &ai) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
