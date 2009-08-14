// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ProxyResolverJSBindingsTest, DnsResolve) {
  scoped_refptr<net::MockHostResolver> host_resolver(new net::MockHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<net::ProxyResolverJSBindings> bindings(
      net::ProxyResolverJSBindings::CreateDefault(host_resolver, NULL));

  // Empty string is not considered a valid host (even though on some systems
  // requesting this will resolve to localhost).
  EXPECT_EQ("", bindings->DnsResolve(""));

  // Should call through to the HostResolver.
  host_resolver->rules()->AddRule("google.com", "192.168.1.1");
  EXPECT_EQ("192.168.1.1", bindings->DnsResolve("google.com"));

  // Resolve failures should give empty string.
  host_resolver->rules()->AddSimulatedFailure("fail");
  EXPECT_EQ("", bindings->DnsResolve("fail"));

  // TODO(eroman): would be nice to have an IPV6 test here too, but that
  // won't work on all systems.
}

TEST(ProxyResolverV8DefaultBindingsTest, MyIpAddress) {
  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<net::ProxyResolverJSBindings> bindings(
      net::ProxyResolverJSBindings::CreateDefault(
          new net::MockHostResolver, NULL));

  // Our IP address is always going to be 127.0.0.1, since we are using a
  // mock host resolver.
  std::string my_ip_address = bindings->MyIpAddress();

  EXPECT_EQ("127.0.0.1", my_ip_address);
}

}  // namespace
