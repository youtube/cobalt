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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstdint>
#include <tuple>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/nplb/posix_compliance/posix_socket_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace nplb {
namespace {

// IPv4 Address 8.8.8.8

const in_addr_t kExpectedIpv4Addr1 =
    htonl((8 << 24) | (8 << 16) | (8 << 8) | 8);
// IPv4 Address 8.8.4.4
const in_addr_t kExpectedIpv4Addr2 =
    htonl((8 << 24) | (8 << 16) | (4 << 8) | 4);
// IPv6 Address 2001:4860:4860::8888
const unsigned char kExpectedIpv6Addr1[16] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x88};
// IPv6 Address 2001:4860:4860::8844
const unsigned char kExpectedIpv6Addr2[16] = {
    0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0, 0, 0, 0, 0, 0, 0, 0, 0x88, 0x44};

void VerifyGoogleDnsAddress(const struct addrinfo* entry) {
  ASSERT_NE(nullptr, entry);
  if (entry->ai_family == AF_INET) {
    struct sockaddr_in* address =
        reinterpret_cast<struct sockaddr_in*>(entry->ai_addr);
    EXPECT_THAT(address->sin_addr.s_addr,
                testing::AnyOf(kExpectedIpv4Addr1, kExpectedIpv4Addr2));
  } else if (entry->ai_family == AF_INET6) {
    struct sockaddr_in6* address =
        reinterpret_cast<struct sockaddr_in6*>(entry->ai_addr);
    EXPECT_THAT(address->sin6_addr.s6_addr,
                testing::AnyOf(testing::ElementsAreArray(kExpectedIpv6Addr1),
                               testing::ElementsAreArray(kExpectedIpv6Addr2)));
  } else {
    FAIL() << "Expected only IPv4 or IPv6 addresses";
  }
}

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
const char kTestHostName[] = "dns.google";
const char kTestService[] = "443";
const char kLocalhost[] = "localhost";

TEST_P(PosixSocketResolveTest, SunnyDayHints) {
  // Perform a DNS lookup for a host with known IPv4 and IPv6 addresses, and
  // verify the returned address.
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  // When requesting a specific address familiy, some platforms will return
  // an error if the device does not have an interface for the given address
  // family.
  if (result != 0 && hints.ai_family != AF_UNSPEC) {
    GTEST_SKIP() << "Skipping test for unsupported address family";
  }
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  int address_count_ipv4 = 0;
  int address_count_ipv6 = 0;

  for (const struct addrinfo* entry = ai; entry != nullptr;
       entry = entry->ai_next) {
    EXPECT_NE(nullptr, entry->ai_addr);
    VerifyGoogleDnsAddress(entry);
    if (entry->ai_family == AF_INET) {
      ++address_count_ipv4;
    } else if (entry->ai_family == AF_INET6) {
      ++address_count_ipv6;
    }
  }
  // IPv4 addresses are either filtered, or at least two are returned.
  EXPECT_THAT(address_count_ipv4, testing::AnyOf(0, testing::Ge(2)));
  // IPv6 addresses are either filtered, or at least two returned.
  EXPECT_THAT(address_count_ipv6, testing::AnyOf(0, testing::Ge(2)));
  // We expect at least two IP addresses for the test host.
  EXPECT_LT(1, address_count_ipv4 + address_count_ipv6);

  freeaddrinfo(ai);
}

TEST_P(PosixSocketResolveTest, SunnyDayFiltered) {
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kTestHostName, 0, &hints, &ai);
  if (GetAddressFamily() == AF_UNSPEC) {
    ASSERT_EQ(result, 0);
    ASSERT_NE(nullptr, ai);
  } else {
    // Requests with a specified address family are allowed to return that there
    // are no matches.
    if (result == 0) {
      ASSERT_NE(nullptr, ai);
    } else {
#if defined(EAI_ADDRFAMILY)
      EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                  result == EAI_FAMILY || result == EAI_ADDRFAMILY)
          << " result = " << result;
#else
#if defined(WSANO_DATA)
      EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                  result == WSANO_DATA || result == EAI_FAMILY)
          << " result = " << result;
#else
      EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                  result == EAI_FAMILY)
          << " result = " << result;
#endif
#endif
      ASSERT_EQ(nullptr, ai);
    }
  }

  for (const struct addrinfo* entry = ai; entry != nullptr;
       entry = entry->ai_next) {
    if (GetAddressFamily() != AF_UNSPEC) {
      EXPECT_EQ(entry->ai_addr->sa_family, GetAddressFamily());
    } else {
      EXPECT_TRUE(entry->ai_addr->sa_family == AF_INET ||
                  entry->ai_addr->sa_family == AF_INET6);
    }
    if (GetSocketType() != 0) {
      EXPECT_EQ(entry->ai_socktype, GetSocketType());
    }
    if (GetProtocol() != 0) {
      EXPECT_EQ(entry->ai_protocol, GetProtocol());
    }
    VerifyGoogleDnsAddress(entry);
  }

  freeaddrinfo(ai);
}

TEST_P(PosixSocketResolveTest, SunnyDayFlags) {
  struct addrinfo hints = {0};
  int flags_to_test[] = {
  // Non-modular builds use native libc getaddrinfo.
#if defined(SB_MODULAR_BUILD)
      // And bionic does not support these flags.
      AI_V4MAPPED, AI_V4MAPPED | AI_ALL, AI_NUMERICHOST, AI_NUMERICSERV,
#endif
      AI_PASSIVE,  AI_CANONNAME,         AI_ADDRCONFIG,
  };
  for (auto flag : flags_to_test) {
    hints.ai_family = GetAddressFamily();
    hints.ai_socktype = GetSocketType();
    hints.ai_protocol = GetProtocol();
    hints.ai_flags = flag;
    struct addrinfo* ai = nullptr;

    int result =
        getaddrinfo((flag & AI_PASSIVE) ? nullptr : kTestHostName,
                    (flag & AI_PASSIVE) ? kTestService : nullptr, &hints, &ai);
    if (GetAddressFamily() == AF_UNSPEC) {
      ASSERT_EQ(result, 0);
      ASSERT_NE(nullptr, ai);
    } else {
      // Requests with a specified address family are allowed to return that
      // there are no matches, unless it's AF_INET6 with AI_V4MAPPED.
      if (result == 0 ||
          (GetAddressFamily() == AF_INET6 && flag == AI_V4MAPPED)) {
        ASSERT_EQ(result, 0);
        ASSERT_NE(nullptr, ai);
      } else {
#if defined(EAI_ADDRFAMILY)
        EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                    result == EAI_FAMILY || result == EAI_ADDRFAMILY)
            << " result = " << result;
#else
#if defined(WSANO_DATA)
        EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                    result == WSANO_DATA || result == EAI_FAMILY)
            << " result = " << result;
#else
        EXPECT_TRUE(result == EAI_NONAME || result == EAI_NODATA ||
                    result == EAI_FAMILY)
            << " result = " << result;
#endif
#endif
        ASSERT_EQ(nullptr, ai);
      }
    }

    for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
      // Not all platforms report these flags with the results.
      if (hints.ai_flags & ~(AI_PASSIVE | AI_CANONNAME | AI_ADDRCONFIG)) {
        EXPECT_EQ(hints.ai_flags, i->ai_flags);
      }
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

TEST_P(PosixSocketResolveTest, Localhost) {
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  struct addrinfo* ai = nullptr;

  int result = getaddrinfo(kLocalhost, 0, &hints, &ai);
  if (result == -1) {
    SB_LOG(INFO) << "getaddrinfo() failed with errno=" << errno << " "
                 << strerror(errno) << " and result=" << result;
  }
  EXPECT_EQ(result, 0);
  ASSERT_NE(nullptr, ai);

  int address_count = 0;
  bool checked_at_least_one = false;
  for (const struct addrinfo* i = ai; i != nullptr; i = i->ai_next) {
    ++address_count;
    if (i->ai_addr == nullptr) {
      continue;
    }
    checked_at_least_one = true;

    if (GetAddressFamily() != AF_UNSPEC) {
      EXPECT_EQ(i->ai_addr->sa_family, GetAddressFamily());
    }

    if (i->ai_addr->sa_family == AF_INET) {
      auto* v4_addr = reinterpret_cast<struct sockaddr_in*>(i->ai_addr);
      EXPECT_EQ(v4_addr->sin_addr.s_addr, htonl(INADDR_LOOPBACK));

      struct in_addr expected_addr;
      expected_addr.s_addr = htonl((127 << 24) | 1);
      EXPECT_EQ(v4_addr->sin_addr.s_addr, expected_addr.s_addr);
    } else if (i->ai_addr->sa_family == AF_INET6) {
      auto* v6_addr = reinterpret_cast<struct sockaddr_in6*>(i->ai_addr);
      struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
      EXPECT_EQ(memcmp(&v6_addr->sin6_addr, &loopback, sizeof(loopback)), 0);

      const unsigned char kExpectedAddress[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                  0, 0, 0, 0, 0, 0, 0, 1};
      EXPECT_THAT(v6_addr->sin6_addr.s6_addr,
                  testing::ElementsAreArray(kExpectedAddress));
    } else {
      FAIL() << "Unexpected address family: " << i->ai_addr->sa_family;
    }
  }
  EXPECT_LT(0, address_count);
  EXPECT_TRUE(checked_at_least_one);

  freeaddrinfo(ai);
}

TEST_P(PosixSocketResolveTest, RainyDayNullHostname) {
  struct addrinfo hints = {0};
  hints.ai_family = GetAddressFamily();
  hints.ai_socktype = GetSocketType();
  hints.ai_protocol = GetProtocol();
  hints.ai_flags = AI_ADDRCONFIG;
  struct addrinfo* ai = nullptr;

  EXPECT_FALSE(getaddrinfo(nullptr, nullptr, &hints, &ai) == 0);
}

#if SB_HAS(IPV6)
INSTANTIATE_TEST_SUITE_P(
    PosixSocketHints,
    PosixSocketResolveTest,
    ::testing::Combine(
        ::testing::Values(AF_UNSPEC, AF_INET, AF_INET6),
        ::testing::Values(std::make_pair(0, 0),
                          std::make_pair(0, IPPROTO_UDP),
                          std::make_pair(0, IPPROTO_TCP),
                          std::make_pair(SOCK_STREAM, 0),
                          std::make_pair(SOCK_DGRAM, 0),
                          std::make_pair(SOCK_DGRAM, IPPROTO_UDP),
                          std::make_pair(SOCK_STREAM, IPPROTO_TCP))),
    GetPosixSocketHintsName);
#else
INSTANTIATE_TEST_SUITE_P(
    PosixSocketHints,
    PosixSocketResolveTest,
    ::testing::Combine(
        ::testing::Values(AF_UNSPEC, AF_INET),
        ::testing::Values(std::make_pair(0, 0),
                          std::make_pair(0, IPPROTO_UDP),
                          std::make_pair(0, IPPROTO_TCP),
                          std::make_pair(SOCK_STREAM, 0),
                          std::make_pair(SOCK_DGRAM, 0),
                          std::make_pair(SOCK_DGRAM, IPPROTO_UDP),
                          std::make_pair(SOCK_STREAM, IPPROTO_TCP))),
    GetPosixSocketHintsName);
#endif

}  // namespace
}  // namespace nplb
