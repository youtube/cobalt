// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mapped_host_resolver.h"

#include "net/base/address_list.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(MappedHostResolverTest, Inclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  MockHostResolver* resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddSimulatedFailure("*google.com");
  resolver_impl->rules()->AddRule("baz.com", "192.168.1.5");
  resolver_impl->rules()->AddRule("foo.com", "192.168.1.8");
  resolver_impl->rules()->AddRule("proxy", "192.168.1.11");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl));

  int rv;
  AddressList address_list;

  // Try resolving "www.google.com:80". There are no mappings yet, so this
  // hits |resolver_impl| and fails.
  TestCompletionCallback callback;
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("www.google.com", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);

  // Remap *.google.com to baz.com.
  EXPECT_TRUE(resolver->AddRuleFromString("map *.google.com baz.com"));

  // Try resolving "www.google.com:80". Should be remapped to "baz.com:80".
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("www.google.com", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.5", NetAddressToString(address_list.head()));
  EXPECT_EQ(80, address_list.GetPort());

  // Try resolving "foo.com:77". This will NOT be remapped, so result
  // is "foo.com:77".
  rv = resolver->Resolve(HostResolver::RequestInfo(HostPortPair("foo.com", 77)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.8", NetAddressToString(address_list.head()));
  EXPECT_EQ(77, address_list.GetPort());

  // Remap "*.org" to "proxy:99".
  EXPECT_TRUE(resolver->AddRuleFromString("Map *.org proxy:99"));

  // Try resolving "chromium.org:61". Should be remapped to "proxy:99".
  rv = resolver->Resolve(HostResolver::RequestInfo
                             (HostPortPair("chromium.org", 61)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.11", NetAddressToString(address_list.head()));
  EXPECT_EQ(99, address_list.GetPort());
}

// Tests that exclusions are respected.
TEST(MappedHostResolverTest, Exclusion) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  MockHostResolver* resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.5");
  resolver_impl->rules()->AddRule("www.google.com", "192.168.1.3");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl));

  int rv;
  AddressList address_list;
  TestCompletionCallback callback;

  // Remap "*.com" to "baz".
  EXPECT_TRUE(resolver->AddRuleFromString("map *.com baz"));

  // Add an exclusion for "*.google.com".
  EXPECT_TRUE(resolver->AddRuleFromString("EXCLUDE *.google.com"));

  // Try resolving "www.google.com". Should not be remapped due to exclusion).
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("www.google.com", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.3", NetAddressToString(address_list.head()));
  EXPECT_EQ(80, address_list.GetPort());

  // Try resolving "chrome.com:80". Should be remapped to "baz:80".
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("chrome.com", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.5", NetAddressToString(address_list.head()));
  EXPECT_EQ(80, address_list.GetPort());
}

TEST(MappedHostResolverTest, SetRulesFromString) {
  // Create a mock host resolver, with specific hostname to IP mappings.
  MockHostResolver* resolver_impl(new MockHostResolver());
  resolver_impl->rules()->AddRule("baz", "192.168.1.7");
  resolver_impl->rules()->AddRule("bar", "192.168.1.9");

  // Create a remapped resolver that uses |resolver_impl|.
  scoped_ptr<MappedHostResolver> resolver(
      new MappedHostResolver(resolver_impl));

  int rv;
  AddressList address_list;
  TestCompletionCallback callback;

  // Remap "*.com" to "baz", and *.net to "bar:60".
  resolver->SetRulesFromString("map *.com baz , map *.net bar:60");

  // Try resolving "www.google.com". Should be remapped to "baz".
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("www.google.com", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.7", NetAddressToString(address_list.head()));
  EXPECT_EQ(80, address_list.GetPort());

  // Try resolving "chrome.net:80". Should be remapped to "bar:60".
  rv = resolver->Resolve(HostResolver::RequestInfo(
                             HostPortPair("chrome.net", 80)),
                         &address_list, callback.callback(), NULL,
                         BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("192.168.1.9", NetAddressToString(address_list.head()));
  EXPECT_EQ(60, address_list.GetPort());
}

// Parsing bad rules should silently discard the rule (and never crash).
TEST(MappedHostResolverTest, ParseInvalidRules) {
  scoped_ptr<MappedHostResolver> resolver(new MappedHostResolver(NULL));

  EXPECT_FALSE(resolver->AddRuleFromString("xyz"));
  EXPECT_FALSE(resolver->AddRuleFromString(""));
  EXPECT_FALSE(resolver->AddRuleFromString(" "));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("EXCLUDE foo bar"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x"));
  EXPECT_FALSE(resolver->AddRuleFromString("INCLUDE x :10"));
}

}  // namespace

}  // namespace net
