// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class TransportSecurityStateTest : public testing::Test {
};

TEST_F(TransportSecurityStateTest, BogusHeaders) {
  int max_age = 42;
  bool include_subdomains = false;

  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "    ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "abc", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  max-age  ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age=   ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     xy", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     3488a923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488a923  ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-ag=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-aged=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age==3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "amax-age=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=-3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923;", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923     e", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923     includesubdomain", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923=includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainx", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=true", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainsx", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomains x", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=34889.23 includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=34889 includesubdomains", &max_age, &include_subdomains));

  EXPECT_EQ(max_age, 42);
  EXPECT_FALSE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, ValidHeaders) {
  int max_age = 42;
  bool include_subdomains = true;

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=243", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 243);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  Max-agE    = 567", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 567);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  mAx-aGe    = 890      ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 890);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=123;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 123);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=394082;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 394082);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age,
            std::min(TransportSecurityState::kMaxHSTSAgeSecs, 39408299l));
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=394082038  ;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age,
            std::min(TransportSecurityState::kMaxHSTSAgeSecs, 394082038l));
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 0);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   ",
      &max_age, &include_subdomains));
  EXPECT_EQ(max_age, TransportSecurityState::kMaxHSTSAgeSecs);
  EXPECT_TRUE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.expiry = expiry;
  state->EnableHost("GOOgle.coM", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "GOOgle.coM"));
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "GOOgle.coM"));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  TransportSecurityState::DomainState domain_state;
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
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityStateTest, Serialise2) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state->EnableHost("google.com", domain_state);

  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output, &dirty));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.google.com"));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.bar.google.com"));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.baz.google.com"));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "com"));
}

TEST_F(TransportSecurityStateTest, Serialise3) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.mode = TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);

  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->Deserialise(output, &dirty));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_OPPORTUNISTIC);
}

TEST_F(TransportSecurityStateTest, DeleteSince) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);

  state->DeleteSince(expiry);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  state->DeleteSince(older);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, DeleteHost) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state->EnableHost("google.com", domain_state);

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "google.com"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "example.com"));
  EXPECT_TRUE(state->DeleteHost("google.com"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com"));
}

TEST_F(TransportSecurityStateTest, SerialiseOld) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  std::string output =
      "{ "
        "\"NiyD+3J1r6z1wjl2n1ALBu94Zj9OsEAMo0kCN8js0Uk=\": {"
          "\"expiry\": 1266815027.983453, "
          "\"include_subdomains\": false, "
          "\"mode\": \"strict\" "
        "}"
      "}";
  bool dirty;
  EXPECT_TRUE(state->Deserialise(output, &dirty));
  EXPECT_TRUE(dirty);
}

TEST_F(TransportSecurityStateTest, IsPreloaded) {
  const std::string paypal =
      TransportSecurityState::CanonicalizeHost("paypal.com");
  const std::string www_paypal =
      TransportSecurityState::CanonicalizeHost("www.paypal.com");
  const std::string a_www_paypal =
      TransportSecurityState::CanonicalizeHost("a.www.paypal.com");
  const std::string abc_paypal =
      TransportSecurityState::CanonicalizeHost("a.b.c.paypal.com");
  const std::string example =
      TransportSecurityState::CanonicalizeHost("example.com");
  const std::string aypal =
      TransportSecurityState::CanonicalizeHost("aypal.com");

  bool b;
  EXPECT_FALSE(TransportSecurityState::IsPreloadedSTS(paypal, &b));
  EXPECT_TRUE(TransportSecurityState::IsPreloadedSTS(www_paypal, &b));
  EXPECT_FALSE(b);
  EXPECT_FALSE(TransportSecurityState::IsPreloadedSTS(a_www_paypal, &b));
  EXPECT_FALSE(TransportSecurityState::IsPreloadedSTS(abc_paypal, &b));
  EXPECT_FALSE(TransportSecurityState::IsPreloadedSTS(example, &b));
  EXPECT_FALSE(TransportSecurityState::IsPreloadedSTS(aypal, &b));
}

TEST_F(TransportSecurityStateTest, Preloaded) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "paypal.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.paypal.com"));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(domain_state.preloaded);
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www2.paypal.com"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "a.www.paypal.com"));

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "elanex.biz"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.elanex.biz"));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "foo.elanex.biz"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "a.foo.elanex.biz"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "sunshinepress.org"));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.sunshinepress.org"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "a.b.sunshinepress.org"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.noisebridge.net"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "noisebridge.net"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "foo.noisebridge.net"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "neg9.org"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.neg9.org"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "riseup.net"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.riseup.net"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "factor.cc"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.factor.cc"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "members.mayfirst.org"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "support.mayfirst.org"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "id.mayfirst.org"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "lists.mayfirst.org"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.mayfirst.org"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "splendidbacon.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.splendidbacon.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.splendidbacon.com"));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "chrome.google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "checkout.google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "health.google.com"));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "aladdinschools.appspot.com"));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState);
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::DomainState domain_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, kLongName));
}

}  // namespace net
