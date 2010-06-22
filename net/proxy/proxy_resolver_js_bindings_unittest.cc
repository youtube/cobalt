// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// This is a HostResolver that synchronously resolves all hosts to the
// following address list of length 3:
//     192.168.1.1
//     172.22.34.1
//     200.100.1.2
class MockHostResolverWithMultipleResults : public HostResolver {
 public:
  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) {
    // Build up the result list (in reverse).
    AddressList temp_list = ResolveIPLiteral("200.100.1.2");
    temp_list = PrependAddressToList("172.22.34.1", temp_list);
    temp_list = PrependAddressToList("192.168.1.1", temp_list);
    *addresses = temp_list;
    return OK;
  }
  virtual void CancelRequest(RequestHandle req) {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual void Shutdown() {}

 private:
  ~MockHostResolverWithMultipleResults() {}

  // Resolves an IP literal to an address list.
  AddressList ResolveIPLiteral(const char* ip_literal) {
    AddressList result;
    int rv = SystemHostResolverProc(ip_literal,
                                    ADDRESS_FAMILY_UNSPECIFIED,
                                    0,
                                    &result, NULL);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(NULL, result.head()->ai_next);
    return result;
  }

  // Builds an AddressList that is |ip_literal| + |address_list|.
  // |orig_list| must not be empty.
  AddressList PrependAddressToList(const char* ip_literal,
                                   const AddressList& orig_list) {
    // Build an addrinfo for |ip_literal|.
    AddressList result = ResolveIPLiteral(ip_literal);

    struct addrinfo* result_head = const_cast<struct addrinfo*>(result.head());

    // Temporarily append |orig_list| to |result|.
    result_head->ai_next = const_cast<struct addrinfo*>(orig_list.head());

    // Make a copy of the concatenated list.
    AddressList concatenated;
    concatenated.Copy(result.head(), true);

    // Restore |result| (so it is freed properly).
    result_head->ai_next = NULL;

    return concatenated;
  }
};

class MockFailingHostResolver : public HostResolver {
 public:
  MockFailingHostResolver() : count_(0) {}

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log) {
    count_++;
    return ERR_NAME_NOT_RESOLVED;
  }

  virtual void CancelRequest(RequestHandle req) {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual void Shutdown() {}

  // Returns the number of times Resolve() has been called.
  int count() const { return count_; }
  void ResetCount() { count_ = 0; }

 private:
  int count_;
};

TEST(ProxyResolverJSBindingsTest, DnsResolve) {
  scoped_refptr<MockHostResolver> host_resolver(new MockHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(host_resolver));

  // Empty string is not considered a valid host (even though on some systems
  // requesting this will resolve to localhost).
  host_resolver->rules()->AddSimulatedFailure("");
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
      ProxyResolverJSBindings::CreateDefault(new MockHostResolver));

  // Our IP address is always going to be 127.0.0.1, since we are using a
  // mock host resolver.
  std::string my_ip_address = bindings->MyIpAddress();

  EXPECT_EQ("127.0.0.1", my_ip_address);
}

// Tests that the regular PAC functions restrict results to IPv4,
// but that the Microsoft extensions to PAC do not. We test this
// by seeing whether ADDRESS_FAMILY_IPV4 or ADDRESS_FAMILY_UNSPECIFIED
// was passed into to the host resolver.
//
//   Restricted to IPv4 address family:
//     myIpAddress()
//     dnsResolve()
//
//   Unrestricted address family:
//     myIpAddressEx()
//     dnsResolveEx()
TEST(ProxyResolverJSBindingsTest, RestrictAddressFamily) {
  scoped_refptr<MockHostResolver> host_resolver(new MockHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(host_resolver));

  // Make it so requests resolve to particular address patterns based on family:
  //  IPV4_ONLY --> 192.168.1.*
  //  UNSPECIFIED --> 192.168.2.1
  host_resolver->rules()->AddRuleForAddressFamily(
      "foo", ADDRESS_FAMILY_IPV4, "192.168.1.1");
  host_resolver->rules()->AddRuleForAddressFamily(
      "*", ADDRESS_FAMILY_IPV4, "192.168.1.2");
  host_resolver->rules()->AddRuleForAddressFamily(
      "foo", ADDRESS_FAMILY_UNSPECIFIED, "192.168.2.1");
  host_resolver->rules()->AddRuleForAddressFamily(
      "*", ADDRESS_FAMILY_UNSPECIFIED, "192.168.2.2");

  // Verify that our mock setups works as expected, and we get different results
  // depending if the address family was IPV4_ONLY or not.
  HostResolver::RequestInfo info("foo", 80);
  AddressList address_list;
  EXPECT_EQ(OK, host_resolver->Resolve(info, &address_list, NULL, NULL,
                                       BoundNetLog()));
  EXPECT_EQ("192.168.2.1", NetAddressToString(address_list.head()));

  info.set_address_family(ADDRESS_FAMILY_IPV4);
  EXPECT_EQ(OK, host_resolver->Resolve(info, &address_list, NULL, NULL,
                                       BoundNetLog()));
  EXPECT_EQ("192.168.1.1", NetAddressToString(address_list.head()));

  // Now the actual test.
  EXPECT_EQ("192.168.1.2", bindings->MyIpAddress());       // IPv4 restricted.
  EXPECT_EQ("192.168.1.1", bindings->DnsResolve("foo"));   // IPv4 restricted.
  EXPECT_EQ("192.168.1.2", bindings->DnsResolve("foo2"));  // IPv4 restricted.

  EXPECT_EQ("192.168.2.2", bindings->MyIpAddressEx());       // Unrestricted.
  EXPECT_EQ("192.168.2.1", bindings->DnsResolveEx("foo"));   // Unrestricted.
  EXPECT_EQ("192.168.2.2", bindings->DnsResolveEx("foo2"));  // Unrestricted.
}

// Test that myIpAddressEx() and dnsResolveEx() both return a semi-colon
// separated list of addresses (as opposed to the non-Ex versions which
// just return the first result).
TEST(ProxyResolverJSBindingsTest, ExFunctionsReturnList) {
  scoped_refptr<HostResolver> host_resolver(
      new MockHostResolverWithMultipleResults);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(host_resolver));

  EXPECT_EQ("192.168.1.1;172.22.34.1;200.100.1.2",
            bindings->MyIpAddressEx());

  EXPECT_EQ("192.168.1.1;172.22.34.1;200.100.1.2",
            bindings->DnsResolveEx("FOO"));
}

TEST(ProxyResolverJSBindingsTest, PerRequestDNSCache) {
  scoped_refptr<MockFailingHostResolver> host_resolver(
      new MockFailingHostResolver);

  // Get a hold of a DefaultJSBindings* (it is a hidden impl class).
  scoped_ptr<ProxyResolverJSBindings> bindings(
      ProxyResolverJSBindings::CreateDefault(host_resolver));

  // Call DnsResolve() 4 times for the same hostname -- this should issue
  // 4 separate calls to the underlying host resolver, since there is no
  // current request context.
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ(4, host_resolver->count());

  host_resolver->ResetCount();

  // Now setup a per-request context, and try the same experiment -- we
  // expect the underlying host resolver to receive only 1 request this time,
  // since it will service the others from the per-request DNS cache.
  HostCache cache(50,
                  base::TimeDelta::FromMinutes(10),
                  base::TimeDelta::FromMinutes(10));
  ProxyResolverRequestContext context(NULL, NULL, &cache);
  bindings->set_current_request_context(&context);

  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ("", bindings->DnsResolve("foo"));
  EXPECT_EQ(1, host_resolver->count());

  host_resolver->ResetCount();

  // The "Ex" version shares this same cache, however since the flags
  // are different it won't reuse this particular entry.
  EXPECT_EQ("", bindings->DnsResolveEx("foo"));
  EXPECT_EQ(1, host_resolver->count());
  EXPECT_EQ("", bindings->DnsResolveEx("foo"));
  EXPECT_EQ("", bindings->DnsResolveEx("foo"));
  EXPECT_EQ(1, host_resolver->count());

  bindings->set_current_request_context(NULL);
}

}  // namespace
}  // namespace net
