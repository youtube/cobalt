// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_key.h"

#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "url/gurl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Check for equality of session keys, and inequality when various pieces of the
// key differ. The SocketTag is only used on Android, and the NAK is only used
// when network partitioning is enabled.
TEST(SpdySessionKeyTest, Equality) {
  SpdySessionKey key(HostPortPair("www.example.org", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow);
  EXPECT_EQ(key,
            SpdySessionKey(HostPortPair("www.example.org", 80),
                           ProxyChain::Direct(), PRIVACY_MODE_DISABLED,
                           SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow));
  EXPECT_NE(key,
            SpdySessionKey(HostPortPair("otherproxy", 80), ProxyChain::Direct(),
                           PRIVACY_MODE_DISABLED,
                           SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow));
  EXPECT_NE(key, SpdySessionKey(
                     HostPortPair("www.example.org", 80),
                     ProxyChain::FromSchemeHostAndPort(
                         ProxyServer::Scheme::SCHEME_HTTPS, "otherproxy", 443),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow));
  EXPECT_NE(key,
            SpdySessionKey(HostPortPair("www.example.org", 80),
                           ProxyChain::Direct(), PRIVACY_MODE_ENABLED,
                           SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow));
  EXPECT_NE(key,
            SpdySessionKey(HostPortPair("www.example.org", 80),
                           ProxyChain::Direct(), PRIVACY_MODE_DISABLED,
                           SpdySessionKey::IsProxySession::kTrue, SocketTag(),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE(key, SpdySessionKey(HostPortPair("www.example.org", 80),
                                ProxyChain::Direct(), PRIVACY_MODE_DISABLED,
                                SpdySessionKey::IsProxySession::kFalse,
                                SocketTag(999, 999), NetworkAnonymizationKey(),
                                SecureDnsPolicy::kAllow));
#endif  // BUILDFLAG(IS_ANDROID)
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    EXPECT_NE(
        key, SpdySessionKey(HostPortPair("www.example.org", 80),
                            ProxyChain::Direct(), PRIVACY_MODE_DISABLED,
                            SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                            NetworkAnonymizationKey::CreateSameSite(
                                SchemefulSite(GURL("http://a.test/"))),
                            SecureDnsPolicy::kAllow));
  }
  EXPECT_NE(key, SpdySessionKey(HostPortPair("www.example.org", 80),
                                ProxyChain::Direct(), PRIVACY_MODE_DISABLED,
                                SpdySessionKey::IsProxySession::kFalse,
                                SocketTag(), NetworkAnonymizationKey(),
                                SecureDnsPolicy::kDisable));
}

// The operator< implementation is suitable for storing distinct keys in a set.
TEST(SpdySessionKeyTest, Set) {
  std::vector<SpdySessionKey> session_keys = {
      SpdySessionKey(HostPortPair("www.example.org", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("otherproxy", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("www.example.org", 80),
                     ProxyChain::FromSchemeHostAndPort(
                         ProxyServer::Scheme::SCHEME_HTTPS, "otherproxy", 443),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("www.example.org", 80),
                     ProxyChain({
                         ProxyServer::FromSchemeHostAndPort(
                             ProxyServer::Scheme::SCHEME_HTTPS, "proxy1", 443),
                         ProxyServer::FromSchemeHostAndPort(
                             ProxyServer::Scheme::SCHEME_HTTPS, "proxy2", 443),
                     }),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("www.example.org", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_ENABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("www.example.org", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kTrue, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow),
      SpdySessionKey(HostPortPair("www.example.org", 80), ProxyChain::Direct(),
                     PRIVACY_MODE_DISABLED,
                     SpdySessionKey::IsProxySession::kFalse, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kDisable),
  };
  std::set<SpdySessionKey> key_set(session_keys.begin(), session_keys.end());
  ASSERT_EQ(session_keys.size(), key_set.size());
}

}  // namespace

}  // namespace net
