// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

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
void CreateAddressList(const std::string& hostname,
                       net::AddressList* addrlist, int port) {
#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif
  int rv = SystemHostResolverProc(hostname,
                                  net::ADDRESS_FAMILY_UNSPECIFIED,
                                  addrlist);
  EXPECT_EQ(0, rv);
  addrlist->SetPort(port);
}

void CreateLongAddressList(net::AddressList* addrlist, int port) {
  CreateAddressList("192.168.1.1", addrlist, port);
  net::AddressList second_list;
  CreateAddressList("192.168.1.2", &second_list, port);
  addrlist->Append(second_list.head());
}

TEST(AddressListTest, GetPort) {
  net::AddressList addrlist;
  CreateAddressList("192.168.1.1", &addrlist, 81);
  EXPECT_EQ(81, addrlist.GetPort());

  addrlist.SetPort(83);
  EXPECT_EQ(83, addrlist.GetPort());
}

TEST(AddressListTest, Assignment) {
  net::AddressList addrlist1;
  CreateAddressList("192.168.1.1", &addrlist1, 85);
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
  CreateAddressList("192.168.1.1", &addrlist1, 11);
  EXPECT_EQ(11, addrlist1.GetPort());
  net::AddressList addrlist2;
  CreateAddressList("192.168.1.2", &addrlist2, 12);
  EXPECT_EQ(12, addrlist2.GetPort());

  ASSERT_TRUE(addrlist1.head()->ai_next == NULL);
  addrlist1.Append(addrlist2.head());
  ASSERT_TRUE(addrlist1.head()->ai_next != NULL);

  net::AddressList addrlist3;
  addrlist3.Copy(addrlist1.head()->ai_next, false);
  EXPECT_EQ(12, addrlist3.GetPort());
}

}  // namespace
