// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class ListValue;
}

namespace net {

namespace {

class HttpServerPropertiesImplTest : public testing::Test {
 protected:
  HttpServerPropertiesImpl impl_;
};

TEST_F(HttpServerPropertiesImplTest, InitializeTest) {
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);

  HostPortPair spdy_server_docs("docs.google.com", 443);
  std::string spdy_server_d =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_docs);

  // Check by initializing NULL spdy servers.
  impl_.Initialize(NULL, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing empty spdy servers.
  HttpServerProperties::StringVector spdy_servers;
  impl_.Initialize(&spdy_servers, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 spdy server.
  HttpServerProperties::StringVector spdy_servers1;
  spdy_servers1.push_back(spdy_server_g);
  impl_.Initialize(&spdy_servers1, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 and docs.google.com:443 spdy
  // servers.
  HttpServerProperties::StringVector spdy_servers2;
  spdy_servers2.push_back(spdy_server_g);
  spdy_servers2.push_back(spdy_server_d);
  impl_.Initialize(&spdy_servers2, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));
}

TEST_F(HttpServerPropertiesImplTest, SupportsSpdyTest) {
  HostPortPair spdy_server_empty("", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Add mail.google.com:443 as not supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));

  // Add docs.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_docs("docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));
}

TEST_F(HttpServerPropertiesImplTest, SetSupportsSpdyTest) {
  HostPortPair spdy_server_empty("", 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Make www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Add mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_mail));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
}

TEST_F(HttpServerPropertiesImplTest, DeleteAllTest) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_mail));

  impl_.DeleteAll();
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
}

TEST_F(HttpServerPropertiesImplTest, GetSpdyServerListTest) {
  base::ListValue spdy_server_list;

  // Check there are no spdy_servers.
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(0u, spdy_server_list.GetSize());

  // Check empty server is not added.
  HostPortPair spdy_server_empty("", 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(0u, spdy_server_list.GetSize());

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_mail);

  // Add www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(0u, spdy_server_list.GetSize());

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(1u, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, false);
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(1u, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list);
  EXPECT_EQ(2u, spdy_server_list.GetSize());

  // Verify www.google.com:443 and mail.google.com:443 are in the list.
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_m));
  if (string_value_g.compare(spdy_server_g) == 0) {
    ASSERT_EQ(spdy_server_g, string_value_g);
    ASSERT_EQ(spdy_server_m, string_value_m);
  } else {
    ASSERT_EQ(spdy_server_g, string_value_m);
    ASSERT_EQ(spdy_server_m, string_value_g);
  }
}

}  // namespace

}  // namespace net
