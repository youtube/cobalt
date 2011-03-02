// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Use getaddrinfo() to allocate an addrinfo structure.
int CreateAddressList(const std::string& hostname, int port,
                      net::AddressList* addrlist) {
#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif
  int rv = SystemHostResolverProc(hostname,
                                  net::ADDRESS_FAMILY_UNSPECIFIED,
                                  0,
                                  addrlist, NULL);
  if (rv == 0)
    addrlist->SetPort(port);
  return rv;
}

void CreateLongAddressList(net::AddressList* addrlist, int port) {
  EXPECT_EQ(0, CreateAddressList("192.168.1.1", port, addrlist));
  net::AddressList second_list;
  EXPECT_EQ(0, CreateAddressList("192.168.1.2", port, &second_list));
  addrlist->Append(second_list.head());
}

TEST(AddressListTest, GetPort) {
  net::AddressList addrlist;
  EXPECT_EQ(0, CreateAddressList("192.168.1.1", 81, &addrlist));
  EXPECT_EQ(81, addrlist.GetPort());

  addrlist.SetPort(83);
  EXPECT_EQ(83, addrlist.GetPort());
}

TEST(AddressListTest, Assignment) {
  net::AddressList addrlist1;
  EXPECT_EQ(0, CreateAddressList("192.168.1.1", 85, &addrlist1));
  EXPECT_EQ(85, addrlist1.GetPort());

  // Should reference the same data as addrlist1 -- so when we change addrlist1
  // both are changed.
  net::AddressList addrlist2 = addrlist1;
  EXPECT_EQ(85, addrlist2.GetPort());

  addrlist1.SetPort(80);
  EXPECT_EQ(80, addrlist1.GetPort());
  EXPECT_EQ(80, addrlist2.GetPort());
}

TEST(AddressListTest, CopyRecursive) {
  net::AddressList addrlist1;
  CreateLongAddressList(&addrlist1, 85);
  EXPECT_EQ(85, addrlist1.GetPort());

  net::AddressList addrlist2;
  addrlist2.Copy(addrlist1.head(), true);

  ASSERT_TRUE(addrlist2.head()->ai_next != NULL);

  // addrlist1 is the same as addrlist2 at this point.
  EXPECT_EQ(85, addrlist1.GetPort());
  EXPECT_EQ(85, addrlist2.GetPort());

  // Changes to addrlist1 are not reflected in addrlist2.
  addrlist1.SetPort(70);
  addrlist2.SetPort(90);

  EXPECT_EQ(70, addrlist1.GetPort());
  EXPECT_EQ(90, addrlist2.GetPort());
}

TEST(AddressListTest, CopyNonRecursive) {
  net::AddressList addrlist1;
  CreateLongAddressList(&addrlist1, 85);
  EXPECT_EQ(85, addrlist1.GetPort());

  net::AddressList addrlist2;
  addrlist2.Copy(addrlist1.head(), false);

  ASSERT_TRUE(addrlist2.head()->ai_next == NULL);

  // addrlist1 is the same as addrlist2 at this point.
  EXPECT_EQ(85, addrlist1.GetPort());
  EXPECT_EQ(85, addrlist2.GetPort());

  // Changes to addrlist1 are not reflected in addrlist2.
  addrlist1.SetPort(70);
  addrlist2.SetPort(90);

  EXPECT_EQ(70, addrlist1.GetPort());
  EXPECT_EQ(90, addrlist2.GetPort());
}

TEST(AddressListTest, Append) {
  net::AddressList addrlist1;
  EXPECT_EQ(0, CreateAddressList("192.168.1.1", 11, &addrlist1));
  EXPECT_EQ(11, addrlist1.GetPort());
  net::AddressList addrlist2;
  EXPECT_EQ(0, CreateAddressList("192.168.1.2", 12, &addrlist2));
  EXPECT_EQ(12, addrlist2.GetPort());

  ASSERT_TRUE(addrlist1.head()->ai_next == NULL);
  addrlist1.Append(addrlist2.head());
  ASSERT_TRUE(addrlist1.head()->ai_next != NULL);

  net::AddressList addrlist3;
  addrlist3.Copy(addrlist1.head()->ai_next, false);
  EXPECT_EQ(12, addrlist3.GetPort());
}

static const char* kCanonicalHostname = "canonical.bar.com";

TEST(AddressListTest, Canonical) {
  // Create an addrinfo with a canonical name.
  sockaddr_in address;
  // The contents of address do not matter for this test,
  // so just zero-ing them out for consistency.
  memset(&address, 0x0, sizeof(address));
  struct addrinfo ai;
  memset(&ai, 0x0, sizeof(ai));
  ai.ai_family = AF_INET;
  ai.ai_socktype = SOCK_STREAM;
  ai.ai_addrlen = sizeof(address);
  ai.ai_addr = reinterpret_cast<sockaddr*>(&address);
  ai.ai_canonname = const_cast<char *>(kCanonicalHostname);

  // Copy the addrinfo struct into an AddressList object and
  // make sure it seems correct.
  net::AddressList addrlist1;
  addrlist1.Copy(&ai, true);
  const struct addrinfo* addrinfo1 = addrlist1.head();
  EXPECT_TRUE(addrinfo1 != NULL);
  EXPECT_TRUE(addrinfo1->ai_next == NULL);
  std::string canon_name1;
  EXPECT_TRUE(addrlist1.GetCanonicalName(&canon_name1));
  EXPECT_EQ("canonical.bar.com", canon_name1);

  // Copy the AddressList to another one.
  net::AddressList addrlist2;
  addrlist2.Copy(addrinfo1, true);
  const struct addrinfo* addrinfo2 = addrlist2.head();
  EXPECT_TRUE(addrinfo2 != NULL);
  EXPECT_TRUE(addrinfo2->ai_next == NULL);
  EXPECT_TRUE(addrinfo2->ai_canonname != NULL);
  EXPECT_NE(addrinfo1, addrinfo2);
  EXPECT_NE(addrinfo1->ai_canonname, addrinfo2->ai_canonname);
  std::string canon_name2;
  EXPECT_TRUE(addrlist2.GetCanonicalName(&canon_name2));
  EXPECT_EQ("canonical.bar.com", canon_name2);

  // Make sure that GetCanonicalName correctly returns false
  // when ai_canonname is NULL.
  ai.ai_canonname = NULL;
  net::AddressList addrlist_no_canon;
  addrlist_no_canon.Copy(&ai, true);
  std::string canon_name3 = "blah";
  EXPECT_FALSE(addrlist_no_canon.GetCanonicalName(&canon_name3));
  EXPECT_EQ("blah", canon_name3);
}

TEST(AddressListTest, IPLiteralConstructor) {
  struct TestData {
    std::string ip_address;
    std::string canonical_ip_address;
    bool is_ipv6;
  } tests[] = {
    { "127.0.00.1", "127.0.0.1", false },
    { "192.168.1.1", "192.168.1.1", false },
    { "::1", "::1", true },
    { "2001:db8:0::42", "2001:db8::42", true },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    net::AddressList expected_list;
    int rv = CreateAddressList(tests[i].canonical_ip_address, 80,
                               &expected_list);
    if (tests[i].is_ipv6 && rv != 0) {
      LOG(WARNING) << "Unable to resolve ip literal '" << tests[i].ip_address
                   << "' test skipped.";
      continue;
    }
    ASSERT_EQ(0, rv);
    const struct addrinfo* good_ai = expected_list.head();

    net::IPAddressNumber ip_number;
    net::ParseIPLiteralToNumber(tests[i].ip_address, &ip_number);
    net::AddressList test_list(ip_number, 80, true);
    const struct addrinfo* test_ai = test_list.head();

    EXPECT_EQ(good_ai->ai_family, test_ai->ai_family);
    EXPECT_EQ(good_ai->ai_socktype, test_ai->ai_socktype);
    EXPECT_EQ(good_ai->ai_addrlen, test_ai->ai_addrlen);
    size_t sockaddr_size =
        good_ai->ai_socktype == AF_INET ? sizeof(struct sockaddr_in) :
        good_ai->ai_socktype == AF_INET6 ? sizeof(struct sockaddr_in6) : 0;
    EXPECT_EQ(memcmp(good_ai->ai_addr, test_ai->ai_addr, sockaddr_size), 0);
    EXPECT_EQ(good_ai->ai_next, test_ai->ai_next);
    EXPECT_EQ(strcmp(tests[i].canonical_ip_address.c_str(),
                     test_ai->ai_canonname), 0);
  }
}

TEST(AddressListTest, AddressFromAddrInfo) {
  struct TestData {
    std::string ip_address;
    std::string canonical_ip_address;
    bool is_ipv6;
  } tests[] = {
    { "127.0.00.1", "127.0.0.1", false },
    { "192.168.1.1", "192.168.1.1", false },
    { "::1", "::1", true },
    { "2001:db8:0::42", "2001:db8::42", true },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    net::AddressList expected_list;
    int rv = CreateAddressList(tests[i].canonical_ip_address, 80,
                               &expected_list);
    if (tests[i].is_ipv6 && rv != 0) {
      LOG(WARNING) << "Unable to resolve ip literal '" << tests[i].ip_address
                   << "' test skipped.";
      continue;
    }
    ASSERT_EQ(0, rv);
    const struct addrinfo* good_ai = expected_list.head();

    scoped_ptr<net::AddressList> test_list(
        net::AddressList::CreateAddressListFromSockaddr(good_ai->ai_addr,
                                                        good_ai->ai_addrlen,
                                                        SOCK_STREAM,
                                                        IPPROTO_TCP));
    const struct addrinfo* test_ai = test_list->head();

    EXPECT_EQ(good_ai->ai_family, test_ai->ai_family);
    EXPECT_EQ(good_ai->ai_addrlen, test_ai->ai_addrlen);
    size_t sockaddr_size =
        good_ai->ai_socktype == AF_INET ? sizeof(struct sockaddr_in) :
        good_ai->ai_socktype == AF_INET6 ? sizeof(struct sockaddr_in6) : 0;
    EXPECT_EQ(memcmp(good_ai->ai_addr, test_ai->ai_addr, sockaddr_size), 0);
    EXPECT_EQ(good_ai->ai_next, test_ai->ai_next);
  }
}

}  // namespace
