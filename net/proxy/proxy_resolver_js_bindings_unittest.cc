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
  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<net::ProxyResolverJSBindings> bindings(
      net::ProxyResolverJSBindings::CreateDefault(
          new net::MockHostResolver, NULL));

  // Considered an error.
  EXPECT_EQ("", bindings->DnsResolve(""));

  const struct {
    const char* input;
    const char* expected;
  } tests[] = {
    {"www.google.com", "127.0.0.1"},
    {".", ""},
    {"foo@google.com", ""},
    {"@google.com", ""},
    {"foo:pass@google.com", ""},
    {"%", ""},
    {"www.google.com:80", ""},
    {"www.google.com:", ""},
    {"www.google.com.", ""},
    {"#", ""},
    {"127.0.0.1", ""},
    {"this has spaces", ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string actual = bindings->DnsResolve(tests[i].input);

    // ########################################################################
    // TODO(eroman)
    // ########################################################################
    // THIS TEST IS CURRENTLY FLAWED.
    //
    // Since we are running in unit-test mode, the HostResolve is using a
    // mock HostResolverProc, which will always return 127.0.0.1, without going
    // through the real codepaths.
    //
    // It is important that these tests be run with the real thing, since we
    // need to verify that HostResolver doesn't blow up when you send it
    // weird inputs. This is necessary since the data reach it is UNSANITIZED.
    // It comes directly from the PAC javascript.
    //
    // For now we just check that it maps to 127.0.0.1.
    std::string expected = tests[i].expected;
    if (expected == "")
      expected = "127.0.0.1";
    EXPECT_EQ(expected, actual);
  }
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
