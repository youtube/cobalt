// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(ProxyResolverJSBindingsTest, DnsResolve) {
  scoped_refptr<MockHostResolver> host_resolver(new MockHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(host_resolver, NULL));

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

TEST(ProxyResolverJSBindingsTest, MyIpAddress) {
  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(
          new MockHostResolver, NULL));

  // Our IP address is always going to be 127.0.0.1, since we are using a
  // mock host resolver.
  std::string my_ip_address = bindings->MyIpAddress();

  EXPECT_EQ("127.0.0.1", my_ip_address);
}

// Tests that myIpAddress() and dnsResolve() pass the flag
// ADDRESS_FAMILY_IPV4_ONLY to the host resolver, as we don't want them
// to return IPv6 results.
TEST(ProxyResolverJSBindingsTest, DontUseIPv6) {
  scoped_refptr<MockHostResolver> host_resolver(new MockHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(
          host_resolver, NULL));

  // Make it so requests resolve to particular address patterns based on family:
  //  IPV4_ONLY --> 192.168.1.*
  //  UNSPECIFIED --> 192.168.2.1
  host_resolver->rules()->AddRuleForFamily(
      "foo", ADDRESS_FAMILY_IPV4_ONLY, "192.168.1.1");
  host_resolver->rules()->AddRuleForFamily(
      "*", ADDRESS_FAMILY_IPV4_ONLY, "192.168.1.2");
  host_resolver->rules()->AddRuleForFamily(
      "*", ADDRESS_FAMILY_UNSPECIFIED, "192.168.2.1");

  // Verify that our mock setups works as expected, and we get different results
  // depending if the address family was IPV4_ONLY or not.
  HostResolver::RequestInfo info("foo", 80);
  AddressList address_list;
  EXPECT_EQ(OK, host_resolver->Resolve(info, &address_list, NULL, NULL, NULL));
  EXPECT_EQ("192.168.2.1", NetAddressToString(address_list.head()));

  info.set_address_family(ADDRESS_FAMILY_IPV4_ONLY);
  EXPECT_EQ(OK, host_resolver->Resolve(info, &address_list, NULL, NULL, NULL));
  EXPECT_EQ("192.168.1.1", NetAddressToString(address_list.head()));

  // Now the actual test.
  EXPECT_EQ("192.168.1.2", bindings->MyIpAddress());
  EXPECT_EQ("192.168.1.1", bindings->DnsResolve("foo"));
  EXPECT_EQ("192.168.1.2", bindings->DnsResolve("foo2"));
}

}  // namespace
}  // namespace net
