// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

namespace net {

namespace {

struct TestData {
  std::string host;
  bool ipv6;
  IPAddressNumber ip_address;
} tests[] = {
  { "127.0.00.1", false},
  { "192.168.1.1", false },
  { "::1", true },
  { "2001:db8:0::42", true },
};
int test_count = ARRAYSIZE_UNSAFE(tests);

class IPEndPointTest : public PlatformTest {
 public:
  virtual void SetUp() {
    // This is where we populate the TestData.
    for (int index = 0; index < test_count; ++index) {
      EXPECT_TRUE(ParseIPLiteralToNumber(tests[index].host,
          &tests[index].ip_address));
    }
  }
};

TEST_F(IPEndPointTest, Constructor) {
  IPEndPoint endpoint;
  EXPECT_EQ(0, endpoint.port());

  for (int index = 0; index < test_count; ++index) {
    IPEndPoint endpoint(tests[index].ip_address, 80);
    EXPECT_EQ(80, endpoint.port());
    EXPECT_EQ(tests[index].ip_address, endpoint.address());
  }
}

TEST_F(IPEndPointTest, Assignment) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest = src;

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
  }
}

TEST_F(IPEndPointTest, Copy) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest(src);

    EXPECT_EQ(src.port(), dest.port());
    EXPECT_EQ(src.address(), dest.address());
  }
}

TEST_F(IPEndPointTest, ToFromSockAddr) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint ip_endpoint(tests[index].ip_address, index);

    // Convert to a sockaddr.
    struct sockaddr_storage addr;
    size_t addr_len = sizeof(addr);
    struct sockaddr* sockaddr = reinterpret_cast<struct sockaddr*>(&addr);
    EXPECT_TRUE(ip_endpoint.ToSockAddr(sockaddr, &addr_len));

    // Basic verification.
    size_t expected_size = tests[index].ipv6 ?
        sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
    EXPECT_EQ(expected_size, addr_len);
    EXPECT_EQ(ip_endpoint.port(), GetPortFromSockaddr(sockaddr, addr_len));

    // And convert back to an IPEndPoint.
    IPEndPoint ip_endpoint2;
    EXPECT_TRUE(ip_endpoint2.FromSockAddr(sockaddr, addr_len));
    EXPECT_EQ(ip_endpoint.port(), ip_endpoint2.port());
    EXPECT_EQ(ip_endpoint.address(), ip_endpoint2.address());
  }
}

TEST_F(IPEndPointTest, ToSockAddrBufTooSmall) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint ip_endpoint(tests[index].ip_address, index);

    struct sockaddr_storage addr;
    size_t addr_len = index;  // size is too small!
    struct sockaddr* sockaddr = reinterpret_cast<struct sockaddr*>(&addr);
    EXPECT_FALSE(ip_endpoint.ToSockAddr(sockaddr, &addr_len));
  }
}

TEST_F(IPEndPointTest, Equality) {
  for (int index = 0; index < test_count; ++index) {
    IPEndPoint src(tests[index].ip_address, index);
    IPEndPoint dest(src);
    EXPECT_TRUE(src == dest);
  }
}

TEST_F(IPEndPointTest, LessThan) {
  // Vary by port.
  IPEndPoint ip_endpoint1(tests[0].ip_address, 100);
  IPEndPoint ip_endpoint2(tests[0].ip_address, 1000);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);

  // IPv4 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 80);
  ip_endpoint2 = IPEndPoint(tests[2].ip_address, 80);
  EXPECT_FALSE(ip_endpoint1 < ip_endpoint2);

  // IPv4 vs IPv4
  ip_endpoint1 = IPEndPoint(tests[0].ip_address, 80);
  ip_endpoint2 = IPEndPoint(tests[1].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);

  // IPv6 vs IPv6
  ip_endpoint1 = IPEndPoint(tests[2].ip_address, 80);
  ip_endpoint2 = IPEndPoint(tests[3].ip_address, 80);
  EXPECT_TRUE(ip_endpoint1 < ip_endpoint2);
}

}  // namespace

}  // namespace net
