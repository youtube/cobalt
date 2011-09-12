// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      new TransportSecurityState(std::string()));
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  state->EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  state->EnableHost("YAhoo.coM", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "YAhoo.coM", true));
  domain_state.expiry = expiry;
  state->EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "YAhoo.coM", true));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state->EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.yahoo.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.yahoo.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.baz.yahoo.com",
                                      true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "com", true));
}

TEST_F(TransportSecurityStateTest, Serialise1) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->LoadEntries(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityStateTest, Serialise2) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state->EnableHost("yahoo.com", domain_state);

  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->LoadEntries(output, &dirty));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.yahoo.com", true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.yahoo.com",
                                      true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.bar.baz.yahoo.com",
                                      true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "com", true));
}

TEST_F(TransportSecurityStateTest, Serialise3) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.mode = TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
  domain_state.expiry = expiry;
  state->EnableHost("yahoo.com", domain_state);

  std::string output;
  bool dirty;
  state->Serialise(&output);
  EXPECT_TRUE(state->LoadEntries(output, &dirty));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_OPPORTUNISTIC);
}

TEST_F(TransportSecurityStateTest, DeleteSince) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state->EnableHost("yahoo.com", domain_state);

  state->DeleteSince(expiry);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  state->DeleteSince(older);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, DeleteHost) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state->EnableHost("yahoo.com", domain_state);

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "example.com", true));
  EXPECT_TRUE(state->DeleteHost("yahoo.com"));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, SerialiseOld) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
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
  EXPECT_TRUE(state->LoadEntries(output, &dirty));
  EXPECT_TRUE(dirty);
}

TEST_F(TransportSecurityStateTest, IsPreloaded) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

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

  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->IsPreloadedSTS(paypal, true, &domain_state));
  EXPECT_TRUE(state->IsPreloadedSTS(www_paypal, true, &domain_state));
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state->IsPreloadedSTS(a_www_paypal, true, &domain_state));
  EXPECT_FALSE(state->IsPreloadedSTS(abc_paypal, true, &domain_state));
  EXPECT_FALSE(state->IsPreloadedSTS(example, true, &domain_state));
  EXPECT_FALSE(state->IsPreloadedSTS(aypal, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, Preloaded) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "paypal.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.paypal.com", true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(domain_state.preloaded);
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www2.paypal.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "a.www.paypal.com",
                                       true));

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "elanex.biz", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.elanex.biz", true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "foo.elanex.biz", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "a.foo.elanex.biz",
                                       true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "sunshinepress.org",
                                      true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.sunshinepress.org",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "a.b.sunshinepress.org",
                                      true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.noisebridge.net",
                                      true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "noisebridge.net",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.noisebridge.net",
                                       true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "neg9.org", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.neg9.org", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "riseup.net", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.riseup.net", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "factor.cc", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.factor.cc", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "members.mayfirst.org",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "support.mayfirst.org",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "id.mayfirst.org", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "lists.mayfirst.org",
                                      true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.mayfirst.org",
                                       true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "splendidbacon.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.splendidbacon.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.splendidbacon.com",
                                      true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "chrome.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "checkout.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "health.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "aladdinschools.appspot.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "ottospora.nl", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.ottospora.nl", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "docs.google.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "sites.google.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "drive.google.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "spreadsheets.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "appengine.google.com",
                                      true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.paycheckrecords.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "market.android.com",
                                      true));
  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_EQ(domain_state.domain, "market.android.com");
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "sub.market.android.com",
                                      true));
  EXPECT_EQ(domain_state.domain, "market.android.com");

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "lastpass.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.lastpass.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "blog.lastpass.com",
                                       true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "keyerror.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.keyerror.com", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "encrypted.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "accounts.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "profiles.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "mail.google.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "chatenabled.mail.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "talkgadget.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "hostedtalkgadget.google.com",
                                      true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "talk.google.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "plus.google.com", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "entropia.de", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.entropia.de", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "foo.entropia.de", true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "ssl.google-analytics.com",
                                      true));

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.google.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.youtube.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "youtube.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "i.ytimg.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "ytimg.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "googleusercontent.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.googleusercontent.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.google-analytics.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "google-analytics.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "googleapis.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "googleadservices.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "googlecode.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "appspot.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "googlesyndication.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "doubleclick.net", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "googlegroups.com",
                                       true));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "gmail.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.gmail.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "m.gmail.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "googlemail.com", true));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.googlemail.com",
                                      true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "m.googlemail.com",
                                       true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "gmail.com", false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.gmail.com", false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "m.gmail.com", false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "googlemail.com", false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.googlemail.com",
                                       false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "m.googlemail.com",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "romab.com", false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.romab.com", false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.romab.com", false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "logentries.com", false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.logentries.com",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.logentries.com",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "stripe.com", false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "foo.stripe.com", false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "cloudsecurityalliance.org",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.cloudsecurityalliance.org",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "login.sapo.pt",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.login.sapo.pt",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "mattmccutchen.net",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.mattmccutchen.net",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "betnet.fr",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.betnet.fr",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "uprotect.it",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.uprotect.it",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "squareup.com",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.squareup.com",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "cert.se",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.cert.se",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "crypto.is",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.crypto.is",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "simon.butcher.name",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.simon.butcher.name",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "linx.net",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.linx.net",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "dropcam.com",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.dropcam.com",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "ebanking.indovinabank.com.vn",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "foo.ebanking.indovinabank.com.vn",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "epoxate.com",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.epoxate.com",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "torproject.org",
                                      false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.torproject.org",
                                      false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "check.torproject.org",
                                      false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "blog.torproject.org",
                                      false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);

  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.torproject.org",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.moneybookers.com",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                      "moneybookers.com",
                                      false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "ledgerscope.net",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.ledgerscope.net",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "status.ledgerscope.net",
                                       false));

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "kyps.net",
                                      false));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "www.kyps.net",
                                      false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "foo.kyps.net",
                                       false));
}

TEST_F(TransportSecurityStateTest, LongNames) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::DomainState domain_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, kLongName, true));
}

TEST_F(TransportSecurityStateTest, PublicKeyHashes) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "example.com", false));
  std::vector<SHA1Fingerprint> hashes;
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(hashes));

  SHA1Fingerprint hash;
  memset(hash.data, '1', sizeof(hash.data));
  domain_state.public_key_hashes.push_back(hash);

  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  hashes.push_back(hash);
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  hashes[0].data[0] = '2';
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.expiry = expiry;
  state->EnableHost("example.com", domain_state);
  std::string ser;
  EXPECT_TRUE(state->Serialise(&ser));
  bool dirty;
  EXPECT_TRUE(state->LoadEntries(ser, &dirty));
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "example.com", false));
  EXPECT_EQ(1u, domain_state.public_key_hashes.size());
  EXPECT_TRUE(0 == memcmp(domain_state.public_key_hashes[0].data, hash.data,
                          sizeof(hash.data)));
}

TEST_F(TransportSecurityStateTest, BuiltinCertPins) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state->IsEnabledForHost(&domain_state,
                                      "chrome.google.com",
                                      true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "chrome.google.com", true));
  std::vector<SHA1Fingerprint> hashes;
  // This essential checks that a built-in list does exist.
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state, "www.paypal.com", true));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state, "twitter.com", true));

  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "docs.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "1.docs.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "sites.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "drive.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "spreadsheets.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "health.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "checkout.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "appengine.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "market.android.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "encrypted.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "accounts.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "profiles.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "mail.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "chatenabled.mail.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "talkgadget.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "hostedtalkgadget.google.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "talk.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "plus.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "ssl.gstatic.com", true));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state, "www.gstatic.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "ssl.google-analytics.com",
                                    true));
}

TEST_F(TransportSecurityStateTest, OptionalHSTSCertPins) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.google-analytics.com",
                                       false));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state,
                                       "www.google-analytics.com",
                                       true));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state,
                                     "www.google-analytics.com",
                                     false));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "www.google-analytics.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "www.google.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "mail-attachment.googleusercontent.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "www.youtube.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "i.ytimg.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "googleapis.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "ajax.googleapis.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "googleadservices.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "pagead2.googleadservices.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "googlecode.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "kibbles.googlecode.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "appspot.com", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state,
                                    "googlesyndication.com",
                                    true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "doubleclick.net", true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "ad.doubleclick.net", true));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state,
                                     "learn.doubleclick.net",
                                     true));
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "a.googlegroups.com", true));
  EXPECT_FALSE(state->HasPinsForHost(&domain_state,
                                     "a.googlegroups.com",
                                     false));
}

TEST_F(TransportSecurityStateTest, ForcePreloads) {
  // This is a docs.google.com override.
  std::string preload("{"
    "\"4AGT3lHihuMSd5rUj7B4u6At0jlSH3HFePovjPR+oLE=\": {"
       "\"created\": 0.0,"
       "\"expiry\": 2000000000.0,"
       "\"include_subdomains\": false,"
       "\"mode\": \"none\""
    "}}");

  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(preload));
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state->HasPinsForHost(&domain_state, "docs.google.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "docs.google.com", true));
}

TEST_F(TransportSecurityStateTest, OverrideBuiltins) {
  scoped_refptr<TransportSecurityState> state(
      new TransportSecurityState(std::string()));

  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state->HasPinsForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state->IsEnabledForHost(&domain_state, "www.google.com", true));

  domain_state = TransportSecurityState::DomainState();
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.expiry = expiry;
  state->EnableHost("www.google.com", domain_state);

  EXPECT_TRUE(state->IsEnabledForHost(&domain_state, "www.google.com", true));
}

}  // namespace net
