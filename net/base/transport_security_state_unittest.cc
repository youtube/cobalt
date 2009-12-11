// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"
#include "testing/gtest/include/gtest/gtest.h"

class TransportSecurityStateTest : public testing::Test {
};

TEST_F(TransportSecurityStateTest, BogusHeaders) {
  int max_age = 42;
  bool include_subdomains = false;

  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "    ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "abc", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "  max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "  max-age  ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age  =", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age=   ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age  =     ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age  =     xy", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "   max-age  =     3488a923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488a923  ", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-ag=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-aged=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age==3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "amax-age=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=-3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923;", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923     e", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923     includesubdomain", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923=includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainx", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=true", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainsx", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomains x", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=34889.23 includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(net::TransportSecurityState::ParseHeader(
      "max-age=34889 includesubdomains", &max_age, &include_subdomains));

  EXPECT_EQ(max_age, 42);
  EXPECT_FALSE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, ValidHeaders) {
  int max_age = 42;
  bool include_subdomains = true;

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "max-age=243", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 243);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "  Max-agE    = 567", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 567);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "  mAx-aGe    = 890      ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 890);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "max-age=123;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 123);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "max-age=394082;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 394082);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 39408299);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "max-age=394082038  ;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 394082038);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(net::TransportSecurityState::ParseHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 0);
  EXPECT_TRUE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);
  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);
  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.expiry = expiry;
  state->EnableHost("GOOgle.coM", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);
  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "GOOgle.coM"));
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "GOOgle.coM"));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);
  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state->EnableHost("google.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.bar.google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.baz.google.com"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "com"));
}

TEST_F(TransportSecurityStateTest, Serialise1) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);
  std::string output;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output));
}

TEST_F(TransportSecurityStateTest, Serialise2) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);

  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.mode = net::TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state->EnableHost("google.com", domain_state);

  std::string output;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_EQ(domain_state.mode, net::TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.google.com"));
  EXPECT_EQ(domain_state.mode, net::TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.bar.google.com"));
  EXPECT_EQ(domain_state.mode, net::TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.baz.google.com"));
  EXPECT_EQ(domain_state.mode, net::TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "com"));
}

TEST_F(TransportSecurityStateTest, Serialise3) {
  scoped_refptr<net::TransportSecurityState> state(
      new net::TransportSecurityState);

  net::TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.mode = net::TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);

  std::string output;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_EQ(domain_state.mode,
            net::TransportSecurityState::DomainState::MODE_OPPORTUNISTIC);
}
