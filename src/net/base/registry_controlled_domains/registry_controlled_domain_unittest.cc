// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "effective_tld_names_unittest1.cc"
#undef TOTAL_KEYWORDS
#undef MIN_WORD_LENGTH
#undef MAX_WORD_LENGTH
#undef MIN_HASH_VALUE
#undef MAX_HASH_VALUE
#include "effective_tld_names_unittest2.cc"

namespace net {
namespace {

std::string GetDomainFromURL(const std::string& url) {
  return RegistryControlledDomainService::GetDomainAndRegistry(GURL(url));
}

std::string GetDomainFromHost(const std::string& host) {
  return RegistryControlledDomainService::GetDomainAndRegistry(host);
}

size_t GetRegistryLengthFromURL(const std::string& url,
                                bool allow_unknown_registries) {
  return RegistryControlledDomainService::GetRegistryLength(GURL(url),
      allow_unknown_registries);
}

size_t GetRegistryLengthFromHost(const std::string& host,
                                 bool allow_unknown_registries) {
  return RegistryControlledDomainService::GetRegistryLength(host,
      allow_unknown_registries);
}

bool CompareDomains(const std::string& url1, const std::string& url2) {
  GURL g1 = GURL(url1);
  GURL g2 = GURL(url2);
  return RegistryControlledDomainService::SameDomainOrHost(g1, g2);
}

}  // namespace

class RegistryControlledDomainTest : public testing::Test {
 protected:
  typedef RegistryControlledDomainService::FindDomainPtr FindDomainPtr;
  void UseDomainData(FindDomainPtr function) {
    RegistryControlledDomainService::UseFindDomainFunction(function);
  }

  virtual void TearDown() {
    RegistryControlledDomainService::UseFindDomainFunction(NULL);
  }
};

TEST_F(RegistryControlledDomainTest, TestGetDomainAndRegistry) {
  UseDomainData(Perfect_Hash_Test1::FindDomain);

  // Test GURL version of GetDomainAndRegistry().
  EXPECT_EQ("baz.jp", GetDomainFromURL("http://a.baz.jp/file.html"));    // 1
  EXPECT_EQ("baz.jp.", GetDomainFromURL("http://a.baz.jp./file.html"));  // 1
  EXPECT_EQ("", GetDomainFromURL("http://ac.jp"));                       // 2
  EXPECT_EQ("", GetDomainFromURL("http://a.bar.jp"));                    // 3
  EXPECT_EQ("", GetDomainFromURL("http://bar.jp"));                      // 3
  EXPECT_EQ("", GetDomainFromURL("http://baz.bar.jp"));                  // 3 4
  EXPECT_EQ("a.b.baz.bar.jp", GetDomainFromURL("http://a.b.baz.bar.jp"));
                                                                         // 4
  EXPECT_EQ("pref.bar.jp", GetDomainFromURL("http://baz.pref.bar.jp"));  // 5
  EXPECT_EQ("b.bar.baz.com.", GetDomainFromURL("http://a.b.bar.baz.com."));
                                                                         // 6
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://a.d.c"));                  // 7
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://.a.d.c"));                 // 7
  EXPECT_EQ("a.d.c", GetDomainFromURL("http://..a.d.c"));                // 7
  EXPECT_EQ("b.c", GetDomainFromURL("http://a.b.c"));                    // 7 8
  EXPECT_EQ("baz.com", GetDomainFromURL("http://baz.com"));              // none
  EXPECT_EQ("baz.com.", GetDomainFromURL("http://baz.com."));            // none

  EXPECT_EQ("", GetDomainFromURL(""));
  EXPECT_EQ("", GetDomainFromURL("http://"));
  EXPECT_EQ("", GetDomainFromURL("file:///C:/file.html"));
  EXPECT_EQ("", GetDomainFromURL("http://foo.com.."));
  EXPECT_EQ("", GetDomainFromURL("http://..."));
  EXPECT_EQ("", GetDomainFromURL("http://192.168.0.1"));
  EXPECT_EQ("", GetDomainFromURL("http://localhost"));
  EXPECT_EQ("", GetDomainFromURL("http://localhost."));
  EXPECT_EQ("", GetDomainFromURL("http:////Comment"));

  // Test std::string version of GetDomainAndRegistry().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ("baz.jp", GetDomainFromHost("a.baz.jp"));                  // 1
  EXPECT_EQ("baz.jp.", GetDomainFromHost("a.baz.jp."));                // 1
  EXPECT_EQ("", GetDomainFromHost("ac.jp"));                           // 2
  EXPECT_EQ("", GetDomainFromHost("a.bar.jp"));                        // 3
  EXPECT_EQ("", GetDomainFromHost("bar.jp"));                          // 3
  EXPECT_EQ("", GetDomainFromHost("baz.bar.jp"));                      // 3 4
  EXPECT_EQ("a.b.baz.bar.jp", GetDomainFromHost("a.b.baz.bar.jp"));    // 3 4
  EXPECT_EQ("pref.bar.jp", GetDomainFromHost("baz.pref.bar.jp"));      // 5
  EXPECT_EQ("b.bar.baz.com.", GetDomainFromHost("a.b.bar.baz.com."));  // 6
  EXPECT_EQ("a.d.c", GetDomainFromHost("a.d.c"));                      // 7
  EXPECT_EQ("a.d.c", GetDomainFromHost(".a.d.c"));                     // 7
  EXPECT_EQ("a.d.c", GetDomainFromHost("..a.d.c"));                    // 7
  EXPECT_EQ("b.c", GetDomainFromHost("a.b.c"));                        // 7 8
  EXPECT_EQ("baz.com", GetDomainFromHost("baz.com"));                  // none
  EXPECT_EQ("baz.com.", GetDomainFromHost("baz.com."));                // none

  EXPECT_EQ("", GetDomainFromHost(""));
  EXPECT_EQ("", GetDomainFromHost("foo.com.."));
  EXPECT_EQ("", GetDomainFromHost("..."));
  EXPECT_EQ("", GetDomainFromHost("192.168.0.1"));
  EXPECT_EQ("", GetDomainFromHost("localhost."));
  EXPECT_EQ("", GetDomainFromHost(".localhost."));
}

TEST_F(RegistryControlledDomainTest, TestGetRegistryLength) {
  UseDomainData(Perfect_Hash_Test1::FindDomain);

  // Test GURL version of GetRegistryLength().
  EXPECT_EQ(2U, GetRegistryLengthFromURL("http://a.baz.jp/file.html", false));
                                                                        // 1
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://a.baz.jp./file.html", false));
                                                                        // 1
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://ac.jp", false));       // 2
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://a.bar.jp", false));    // 3
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://bar.jp", false));      // 3
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.bar.jp", false));  // 3 4
  EXPECT_EQ(12U, GetRegistryLengthFromURL("http://a.b.baz.bar.jp", false));
                                                                        // 4
  EXPECT_EQ(6U, GetRegistryLengthFromURL("http://baz.pref.bar.jp", false));
                                                                        // 5
  EXPECT_EQ(11U, GetRegistryLengthFromURL("http://a.b.bar.baz.com", false));
                                                                        // 6
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://a.d.c", false));       // 7
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://.a.d.c", false));      // 7
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://..a.d.c", false));     // 7
  EXPECT_EQ(1U, GetRegistryLengthFromURL("http://a.b.c", false));       // 7 8
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.com", false));     // none
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://baz.com.", false));    // none
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://baz.com", true));      // none
  EXPECT_EQ(4U, GetRegistryLengthFromURL("http://baz.com.", true));     // none

  EXPECT_EQ(std::string::npos, GetRegistryLengthFromURL("", false));
  EXPECT_EQ(std::string::npos, GetRegistryLengthFromURL("http://", false));
  EXPECT_EQ(std::string::npos,
            GetRegistryLengthFromURL("file:///C:/file.html", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://foo.com..", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://...", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://192.168.0.1", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost", true));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost.", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://localhost.", true));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http:////Comment", false));

  // Test std::string version of GetRegistryLength().  Uses the same
  // underpinnings as the GURL version, so this is really more of a check of
  // CanonicalizeHost().
  EXPECT_EQ(2U, GetRegistryLengthFromHost("a.baz.jp", false));          // 1
  EXPECT_EQ(3U, GetRegistryLengthFromHost("a.baz.jp.", false));         // 1
  EXPECT_EQ(0U, GetRegistryLengthFromHost("ac.jp", false));             // 2
  EXPECT_EQ(0U, GetRegistryLengthFromHost("a.bar.jp", false));          // 3
  EXPECT_EQ(0U, GetRegistryLengthFromHost("bar.jp", false));            // 3
  EXPECT_EQ(0U, GetRegistryLengthFromHost("baz.bar.jp", false));        // 3 4
  EXPECT_EQ(12U, GetRegistryLengthFromHost("a.b.baz.bar.jp", false));   // 4
  EXPECT_EQ(6U, GetRegistryLengthFromHost("baz.pref.bar.jp", false));   // 5
  EXPECT_EQ(11U, GetRegistryLengthFromHost("a.b.bar.baz.com", false));  // 6
  EXPECT_EQ(3U, GetRegistryLengthFromHost("a.d.c", false));             // 7
  EXPECT_EQ(3U, GetRegistryLengthFromHost(".a.d.c", false));            // 7
  EXPECT_EQ(3U, GetRegistryLengthFromHost("..a.d.c", false));           // 7
  EXPECT_EQ(1U, GetRegistryLengthFromHost("a.b.c", false));             // 7 8
  EXPECT_EQ(0U, GetRegistryLengthFromHost("baz.com", false));           // none
  EXPECT_EQ(0U, GetRegistryLengthFromHost("baz.com.", false));          // none
  EXPECT_EQ(3U, GetRegistryLengthFromHost("baz.com", true));            // none
  EXPECT_EQ(4U, GetRegistryLengthFromHost("baz.com.", true));           // none

  EXPECT_EQ(std::string::npos, GetRegistryLengthFromHost("", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("foo.com..", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("..", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("192.168.0.1", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("localhost", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("localhost", true));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("localhost.", false));
  EXPECT_EQ(0U, GetRegistryLengthFromHost("localhost.", true));
}

TEST_F(RegistryControlledDomainTest, TestSameDomainOrHost) {
  UseDomainData(Perfect_Hash_Test2::FindDomain);

  EXPECT_TRUE(CompareDomains("http://a.b.bar.jp/file.html",
                             "http://a.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_TRUE(CompareDomains("http://a.b.bar.jp/file.html",
                             "http://b.b.bar.jp/file.html"));  // b.bar.jp
  EXPECT_FALSE(CompareDomains("http://a.foo.jp/file.html",     // foo.jp
                              "http://a.not.jp/file.html"));   // not.jp
  EXPECT_FALSE(CompareDomains("http://a.foo.jp/file.html",     // foo.jp
                              "http://a.foo.jp./file.html"));  // foo.jp.
  EXPECT_FALSE(CompareDomains("http://a.com/file.html",        // a.com
                              "http://b.com/file.html"));      // b.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://b.x.com/file.html"));     // x.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://.x.com/file.html"));      // x.com
  EXPECT_TRUE(CompareDomains("http://a.x.com/file.html",
                             "http://..b.x.com/file.html"));   // x.com
  EXPECT_TRUE(CompareDomains("http://intranet/file.html",
                             "http://intranet/file.html"));    // intranet
  EXPECT_TRUE(CompareDomains("http://127.0.0.1/file.html",
                             "http://127.0.0.1/file.html"));   // 127.0.0.1
  EXPECT_FALSE(CompareDomains("http://192.168.0.1/file.html",  // 192.168.0.1
                              "http://127.0.0.1/file.html"));  // 127.0.0.1
  EXPECT_FALSE(CompareDomains("file:///C:/file.html",
                              "file:///C:/file.html"));        // no host
}

TEST_F(RegistryControlledDomainTest, TestDefaultData) {
  // Note that no data is set: we're using the default rules.
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://google.com", false));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://stanford.edu", false));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://ustreas.gov", false));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://icann.net", false));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://ferretcentral.org", false));
  EXPECT_EQ(0U, GetRegistryLengthFromURL("http://nowhere.foo", false));
  EXPECT_EQ(3U, GetRegistryLengthFromURL("http://nowhere.foo", true));
}

}  // namespace net
