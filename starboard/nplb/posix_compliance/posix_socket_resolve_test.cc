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
#include "testing/gtest/include/gtest/gtest.h"

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
      if (i->ai_family == AF_INET) {
        EXPECT_EQ(static_cast<size_t>(i->ai_addrlen),
                  sizeof(struct sockaddr_in));
      } else if (i->ai_family == AF_INET6) {
        EXPECT_EQ(static_cast<size_t>(i->ai_addrlen),
                  sizeof(struct sockaddr_in6));
      }
      break;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_NE(nullptr, ai_addr);

  EXPECT_TRUE(ai_addr->sin_family == AF_INET ||
              ai_addr->sin_family == AF_INET6);

  freeaddrinfo(ai);
}

TEST(PosixSocketResolveTest, SunnyDaySocketType) {
  struct addrinfo hints = {0};
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

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
  // Non-modular builds use native libc getaddrinfo.
#if defined(SB_MODULAR_BUILD)
      // And bionic does not support these flags.
      AI_V4MAPPED, AI_NUMERICHOST, AI_NUMERICSERV,
#endif
      AI_PASSIVE,  AI_CANONNAME,   AI_ADDRCONFIG,
  };
  for (auto flag : flags_to_test) {
    hints.ai_flags = flag;
    hints.ai_socktype = SOCK_STREAM;
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

  int result = getaddrinfo(nullptr, nullptr, &hints, &ai);

  EXPECT_FALSE(result == 0);

  // Since we cannot guarantee the contents of what the string is,
  // (the strings are implementation-defined), we can only expect
  // that the pointer is not null.
  EXPECT_NE(nullptr, gai_strerror(result));
}

}  // namespace
}  // namespace nplb
