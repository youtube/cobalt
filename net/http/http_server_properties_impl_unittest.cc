// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <string>
#include <vector>

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

typedef HttpServerPropertiesImplTest SpdyServerPropertiesTest;

TEST_F(SpdyServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_google);

  HostPortPair spdy_server_docs("docs.google.com", 443);
  std::string spdy_server_d =
      HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_docs);

  // Check by initializing NULL spdy servers.
  impl_.InitializeSpdyServers(NULL, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing empty spdy servers.
  std::vector<std::string> spdy_servers;
  impl_.InitializeSpdyServers(&spdy_servers, true);
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 spdy server.
  std::vector<std::string> spdy_servers1;
  spdy_servers1.push_back(spdy_server_g);
  impl_.InitializeSpdyServers(&spdy_servers1, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));

  // Check by initializing with www.google.com:443 and docs.google.com:443 spdy
  // servers.
  std::vector<std::string> spdy_servers2;
  spdy_servers2.push_back(spdy_server_g);
  spdy_servers2.push_back(spdy_server_d);
  impl_.InitializeSpdyServers(&spdy_servers2, true);
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_docs));
}

TEST_F(SpdyServerPropertiesTest, SupportsSpdyTest) {
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

TEST_F(SpdyServerPropertiesTest, SetSupportsSpdy) {
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

TEST_F(SpdyServerPropertiesTest, Clear) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsSpdy(spdy_server_mail));

  impl_.Clear();
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsSpdy(spdy_server_mail));
}

TEST_F(SpdyServerPropertiesTest, GetSpdyServerList) {
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

typedef HttpServerPropertiesImplTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_FALSE(
      impl_.HasAlternateProtocol(test_host_port_pair));
  impl_.SetAlternateProtocol(
      test_host_port_pair, 443, NPN_SPDY_1);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  const PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(NPN_SPDY_1, alternate.protocol);

  impl_.Clear();
  EXPECT_FALSE(impl_.HasAlternateProtocol(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Initialize) {
  HostPortPair test_host_port_pair1("foo1", 80);
  impl_.SetBrokenAlternateProtocol(test_host_port_pair1);
  HostPortPair test_host_port_pair2("foo2", 80);
  impl_.SetAlternateProtocol(
      test_host_port_pair2, 443, NPN_SPDY_1);

  AlternateProtocolMap alternate_protocol_map;
  PortAlternateProtocolPair port_alternate_protocol_pair;
  port_alternate_protocol_pair.port = 123;
  port_alternate_protocol_pair.protocol = NPN_SPDY_2;
  alternate_protocol_map[test_host_port_pair2] = port_alternate_protocol_pair;
  impl_.InitializeAlternateProtocolServers(&alternate_protocol_map);

  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair1));
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair2));
  port_alternate_protocol_pair =
      impl_.GetAlternateProtocol(test_host_port_pair1);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, port_alternate_protocol_pair.protocol);
  port_alternate_protocol_pair =
      impl_.GetAlternateProtocol(test_host_port_pair2);
  EXPECT_EQ(123, port_alternate_protocol_pair.port);
  EXPECT_EQ(NPN_SPDY_2, port_alternate_protocol_pair.protocol);
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  impl_.SetBrokenAlternateProtocol(test_host_port_pair);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol);

  impl_.SetAlternateProtocol(
      test_host_port_pair,
      1234,
      NPN_SPDY_1);
  alternate = impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(ALTERNATE_PROTOCOL_BROKEN, alternate.protocol)
      << "Second attempt should be ignored.";
}

TEST_F(AlternateProtocolServerPropertiesTest, Forced) {
  // Test forced alternate protocols.

  PortAlternateProtocolPair default_protocol;
  default_protocol.port = 1234;
  default_protocol.protocol = NPN_SPDY_2;
  HttpServerPropertiesImpl::ForceAlternateProtocol(default_protocol);

  // Verify the forced protocol.
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_TRUE(
      impl_.HasAlternateProtocol(test_host_port_pair));
  PortAlternateProtocolPair alternate =
      impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(default_protocol.port, alternate.port);
  EXPECT_EQ(default_protocol.protocol, alternate.protocol);

  // Verify the real protocol overrides the forced protocol.
  impl_.SetAlternateProtocol(
      test_host_port_pair, 443, NPN_SPDY_1);
  ASSERT_TRUE(impl_.HasAlternateProtocol(test_host_port_pair));
  alternate = impl_.GetAlternateProtocol(test_host_port_pair);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(NPN_SPDY_1, alternate.protocol);

  // Turn off the static, forced alternate protocol so that tests don't
  // have this state.
  HttpServerPropertiesImpl::DisableForcedAlternateProtocol();

  // Verify the forced protocol is off.
  HostPortPair test_host_port_pair2("bar", 80);
  EXPECT_FALSE(
      impl_.HasAlternateProtocol(test_host_port_pair2));
}

}  // namespace

}  // namespace net
