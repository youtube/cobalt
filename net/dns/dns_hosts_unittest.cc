// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_hosts.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsHostsTest, ParseHosts) {
  std::string contents =
      "127.0.0.1       localhost\tlocalhost.localdomain # standard\n"
      "\n"
      "1.0.0.1 localhost # ignored, first hit above\n"
      "fe00::x example company # ignored, malformed IPv6\n"
      "1.0.0.300 company # ignored, malformed IPv4\n"
      "1.0.0.1 # ignored, missing hostname\n"
      "1.0.0.1\t company   \n"
      "::1\tlocalhost ip6-localhost ip6-loopback # comment # within a comment\n"
      "\t fe00::0 ip6-localnet\r\n"
      "2048::2 example\n"
      "2048::1 company example # ignored for 'example' \n"
      "gibberish";

  const struct {
    const char* host;
    AddressFamily family;
    const char* ip;
  } entries[] = {
    { "localhost", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "localhost.localdomain", ADDRESS_FAMILY_IPV4, "127.0.0.1" },
    { "company", ADDRESS_FAMILY_IPV4, "1.0.0.1" },
    { "localhost", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-localhost", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-loopback", ADDRESS_FAMILY_IPV6, "::1" },
    { "ip6-localnet", ADDRESS_FAMILY_IPV6, "fe00::0" },
    { "company", ADDRESS_FAMILY_IPV6, "2048::1" },
    { "example", ADDRESS_FAMILY_IPV6, "2048::2" },
  };

  DnsHosts expected;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(entries); ++i) {
    DnsHostsKey key(entries[i].host, entries[i].family);
    IPAddressNumber& ip = expected[key];
    ASSERT_TRUE(ip.empty());
    ASSERT_TRUE(ParseIPLiteralToNumber(entries[i].ip, &ip));
    ASSERT_EQ(ip.size(), (entries[i].family == ADDRESS_FAMILY_IPV4) ? 4u : 16u);
  }

  DnsHosts hosts;
  ParseHosts(contents, &hosts);
  ASSERT_EQ(expected, hosts);
}

}  // namespace

}  // namespace net

