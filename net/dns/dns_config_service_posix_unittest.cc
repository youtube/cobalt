// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <resolv.h>

#include "net/dns/dns_config_service_posix.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CompareConfig(const struct __res_state &res, const DnsConfig& config) {
  EXPECT_EQ(config.ndots, static_cast<int>(res.ndots));
  EXPECT_EQ(config.edns0, (res.options & RES_USE_EDNS0) != 0);
  EXPECT_EQ(config.rotate, (res.options & RES_ROTATE) != 0);
  EXPECT_EQ(config.timeout.InSeconds(), res.retrans);
  EXPECT_EQ(config.attempts, res.retry);

  // Compare nameservers. IPv6 precede IPv4.
#if defined(OS_LINUX)
  size_t nscount6 = res._u._ext.nscount6;
#else
  size_t nscount6 = 0;
#endif
  size_t nscount4 = res.nscount;
  ASSERT_EQ(config.nameservers.size(), nscount6 + nscount4);
#if defined(OS_LINUX)
  for (size_t i = 0; i < nscount6; ++i) {
    IPEndPoint ipe;
    EXPECT_TRUE(ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]),
        sizeof *res._u._ext.nsaddrs[i]));
    EXPECT_EQ(config.nameservers[i], ipe);
  }
#endif
  for (size_t i = 0; i < nscount4; ++i) {
    IPEndPoint ipe;
    EXPECT_TRUE(ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
        sizeof res.nsaddr_list[i]));
    EXPECT_EQ(config.nameservers[nscount6 + i], ipe);
  }

  ASSERT_TRUE(config.search.size() <= MAXDNSRCH);
  EXPECT_TRUE(res.dnsrch[config.search.size()] == NULL);
  for (size_t i = 0; i < config.search.size(); ++i) {
    EXPECT_EQ(config.search[i], res.dnsrch[i]);
  }
}

// Fills in |res| with sane configuration. Change |generation| to add diversity.
void InitializeResState(res_state res, int generation) {
  memset(res, 0, sizeof(*res));
  res->options = RES_INIT | RES_ROTATE;
  res->ndots = 2;
  res->retrans = 8;
  res->retry = 7;

  const char kDnsrch[] = "chromium.org" "\0" "example.com";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;
  res->dnsrch[1] = res->defdname + sizeof("chromium.org");

  const char* ip4addr[3] = {
      "8.8.8.8",
      "192.168.1.1",
      "63.1.2.4",
  };

  for (int i = 0; i < 3; ++i) {
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(NS_DEFAULTPORT + i - generation);
    inet_pton(AF_INET, ip4addr[i], &sa.sin_addr);
    res->nsaddr_list[i] = sa;
  }
  res->nscount = 3;

#if defined(OS_LINUX)
  const char* ip6addr[2] = {
      "2001:db8:0::42",
      "::FFFF:129.144.52.38",
  };

  for (int i = 0; i < 2; ++i) {
    // Must use malloc to mimick res_ninit.
    struct sockaddr_in6 *sa6;
    sa6 = (struct sockaddr_in6 *)malloc(sizeof(*sa6));
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = htons(NS_DEFAULTPORT - i);
    inet_pton(AF_INET6, ip6addr[i], &sa6->sin6_addr);
    res->_u._ext.nsaddrs[i] = sa6;
  }
  res->_u._ext.nscount6 = 2;
#endif
}

void CloseResState(res_state res) {
#if defined(OS_LINUX)
  for (int i = 0; i < res->_u._ext.nscount6; ++i) {
    ASSERT_TRUE(res->_u._ext.nsaddrs[i] != NULL);
    free(res->_u._ext.nsaddrs[i]);
  }
#endif
}

}  // namespace

TEST(DnsConfigTest, ResolverConfigConvertAndEquals) {
  struct __res_state res[2];
  DnsConfig config[2];
  for (int i = 0; i < 2; ++i) {
    EXPECT_FALSE(config[i].IsValid());
    InitializeResState(&res[i], i);
    ASSERT_TRUE(ConvertResToConfig(res[i], &config[i]));
  }
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(config[i].IsValid());
    CompareConfig(res[i], config[i]);
    CloseResState(&res[i]);
  }
  EXPECT_TRUE(config[0].Equals(config[0]));
  EXPECT_FALSE(config[0].Equals(config[1]));
  EXPECT_FALSE(config[1].Equals(config[0]));
}

}  // namespace net


