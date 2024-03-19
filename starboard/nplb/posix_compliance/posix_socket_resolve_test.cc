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

#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/time.h"
#include "starboard/nplb/socket_helpers.h"

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
  EXPECT_TRUE(result == 0);
  ASSERT_NE(nullptr, ai);

  int address_count = 0;
  struct sockaddr* ai_addr = nullptr;

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
    if (ai_addr == nullptr && i->ai_addr != nullptr) {
      ai_addr = i->ai_addr;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_NE(nullptr, ai_addr);

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    EXPECT_TRUE(i->ai_family == AF_INET || i->ai_family == AF_INET6);
  }

  freeaddrinfo(ai);
}

TEST(PosixSocketResolveTest, Localhost) {
  struct addrinfo hints = {0};
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kLocalhost, 0, &hints, &ai);
  EXPECT_TRUE(result == 0);
  ASSERT_NE(nullptr, ai);

  int address_count = 0;
  struct sockaddr* ai_addr = nullptr;
  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
    if (ai_addr == nullptr && i->ai_addr != nullptr) {
      ai_addr = i->ai_addr;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_NE(nullptr, ai_addr);

  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    EXPECT_TRUE(i->ai_family == AF_INET || i->ai_family == AF_INET6);
  }

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
