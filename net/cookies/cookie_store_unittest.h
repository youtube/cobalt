// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_UNITTEST_H_
#define NET_COOKIES_COOKIE_STORE_UNITTEST_H_
#pragma once

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/string_tokenizer.h"
#include "base/threading/thread.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file declares unittest templates that can be used to test common
// behavior of any CookieStore implementation.
// See cookie_monster_unittest.cc for an example of an implementation.

namespace net {

using base::Thread;

const int kTimeout = 1000;

const char kUrlFtp[] = "ftp://ftp.google.izzle/";
const char kUrlGoogle[] = "http://www.google.izzle";
const char kUrlGoogleFoo[] = "http://www.google.izzle/foo";
const char kUrlGoogleBar[] = "http://www.google.izzle/bar";
const char kUrlGoogleSecure[] = "https://www.google.izzle";
const char kValidCookieLine[] = "A=B; path=/";
const char kValidDomainCookieLine[] = "A=B; path=/; domain=google.izzle";

// The CookieStoreTestTraits must have the following members:
// struct CookieStoreTestTraits {
//   // Factory function.
//   static scoped_refptr<CookieStore> Create();
//
//   // The cookie store is a CookieMonster. Only used to test
//   // GetCookieMonster().
//   static const bool is_cookie_monster;
//
//   // The cookie store supports cookies with the exclude_httponly() option.
//   static const bool supports_http_only;
//
//   // The cookie store supports the GetCookiesWithInfoAsync() method.
//   static const bool supports_cookies_with_info;
//
//   // The cookie store is able to make the difference between the ".com"
//   // and the "com" domains.
//   static const bool supports_non_dotted_domains;
//
//   // The cookie store handles the domains with trailing dots (such as "com.")
//   // correctly.
//   static const bool supports_trailing_dots;
//
//   // The cookie store rejects cookies for invalid schemes such as ftp.
//   static const bool filters_schemes;
//
//   // The cookie store has a bug happening when a path is a substring of
//   // another.
//   static const bool has_path_prefix_bug;
//
//   // Time to wait between two cookie insertions to ensure that cookies have
//   // different creation times.
//   static const int creation_time_granularity_in_ms;
// };

template <class CookieStoreTestTraits>
class CookieStoreTest : public testing::Test {
 protected:
  CookieStoreTest()
      : url_google_(kUrlGoogle),
        url_google_secure_(kUrlGoogleSecure),
        url_google_foo_(kUrlGoogleFoo),
        url_google_bar_(kUrlGoogleBar) {
    // This test may be used outside of the net test suite, and thus may not
    // have a message loop.
    if (!MessageLoop::current())
      message_loop_.reset(new MessageLoop);
    weak_factory_.reset(
        new base::WeakPtrFactory<MessageLoop>(MessageLoop::current()));
  }

  // Helper methods for the asynchronous Cookie Store API that call the
  // asynchronous method and then pump the loop until the callback is invoked,
  // finally returning the value.

  std::string GetCookies(CookieStore* cs, const GURL& url) {
    DCHECK(cs);
    CookieOptions options;
    if (!CookieStoreTestTraits::supports_http_only)
      options.set_include_httponly();
    GetCookieStringCallback callback;
    cs->GetCookiesWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieStringCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookie();
  }

  std::string GetCookiesWithOptions(CookieStore* cs,
                                    const GURL& url,
                                    const CookieOptions& options) {
    DCHECK(cs);
    GetCookieStringCallback callback;
    cs->GetCookiesWithOptionsAsync(
        url, options, base::Bind(&GetCookieStringCallback::Run,
                                 base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookie();
  }

  void GetCookiesWithInfo(CookieStore* cs,
                          const GURL& url,
                          const CookieOptions& options,
                          std::string* cookie_line,
                          std::vector<CookieStore::CookieInfo>* cookie_info) {
    DCHECK(cs);
    DCHECK(cookie_line);
    DCHECK(cookie_info);
    GetCookiesWithInfoCallback callback;
    cs->GetCookiesWithInfoAsync(
        url, options, base::Bind(&GetCookiesWithInfoCallback::Run,
                                 base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    *cookie_line = callback.cookie_line();
    *cookie_info = callback.cookie_info();
  }

  bool SetCookieWithOptions(CookieStore* cs,
                            const GURL& url,
                            const std::string& cookie_line,
                            const CookieOptions& options) {
    DCHECK(cs);
    SetCookieCallback callback;
    cs->SetCookieWithOptionsAsync(
        url, cookie_line, options,
        base::Bind(&SetCookieCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  bool SetCookie(CookieStore* cs,
                 const GURL& url,
                 const std::string& cookie_line) {
    CookieOptions options;
    if (!CookieStoreTestTraits::supports_http_only)
      options.set_include_httponly();
    return SetCookieWithOptions(cs, url, cookie_line, options);
  }

  void DeleteCookie(CookieStore* cs,
                    const GURL& url,
                    const std::string& cookie_name) {
    DCHECK(cs);
    DeleteCookieCallback callback;
    cs->DeleteCookieAsync(
        url, cookie_name,
        base::Bind(&DeleteCookieCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
  }

  int DeleteCreatedBetween(CookieStore* cs,
                            const base::Time& delete_begin,
                            const base::Time& delete_end) {
    DCHECK(cs);
    DeleteCallback callback;
    cs->DeleteAllCreatedBetweenAsync(
        delete_begin, delete_end,
        base::Bind(&DeleteCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.num_deleted();
  }

  int DeleteSessionCookies(CookieStore* cs) {
    DCHECK(cs);
    DeleteCallback callback;
    cs->DeleteSessionCookiesAsync(
        base::Bind(&DeleteCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.num_deleted();
  }

  void RunFor(int ms) {
    // Runs the test thread message loop for up to |ms| milliseconds.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&MessageLoop::Quit, weak_factory_->GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(ms));
    MessageLoop::current()->Run();
    weak_factory_->InvalidateWeakPtrs();
  }

  scoped_refptr<CookieStore> GetCookieStore() {
    return CookieStoreTestTraits::Create();
  }

  // Compares two cookie lines.
  void MatchCookieLines(const std::string& line1, const std::string& line2) {
    EXPECT_EQ(TokenizeCookieLine(line1), TokenizeCookieLine(line2));
  }

  // Check the cookie line by polling until equality or a timeout is reached.
  void MatchCookieLineWithTimeout(CookieStore* cs,
                                  const GURL& url,
                                  const std::string& line) {
    std::string cookies = GetCookies(cs, url);
    bool matched = (TokenizeCookieLine(line) == TokenizeCookieLine(cookies));
    base::Time polling_end_date = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(
            CookieStoreTestTraits::creation_time_granularity_in_ms);

    while (!matched &&  base::Time::Now() <= polling_end_date) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
      cookies = GetCookies(cs, url);
      matched = (TokenizeCookieLine(line) == TokenizeCookieLine(cookies));
    }

    EXPECT_TRUE(matched) << "\"" << cookies
                         << "\" does not match \"" << line << "\"";
  }

  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_foo_;
  GURL url_google_bar_;

  scoped_ptr<base::WeakPtrFactory<MessageLoop> > weak_factory_;
  scoped_ptr<MessageLoop> message_loop_;

 private:
  // Returns a set of strings of type "name=value". Fails in case of duplicate.
  std::set<std::string> TokenizeCookieLine(const std::string& line) {
    std::set<std::string> tokens;
    StringTokenizer tokenizer(line, " ;");
    while (tokenizer.GetNext())
      EXPECT_TRUE(tokens.insert(tokenizer.token()).second);
    return tokens;
  }
};

TYPED_TEST_CASE_P(CookieStoreTest);

TYPED_TEST_P(CookieStoreTest, TypeTest) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_EQ(cs->GetCookieMonster(),
            (TypeParam::is_cookie_monster) ?
                static_cast<CookieMonster*>(cs.get()) : NULL);
}

TYPED_TEST_P(CookieStoreTest, DomainTest) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              "C=D; domain=.google.izzle"));
  this->MatchCookieLines("A=B; C=D", this->GetCookies(cs, this->url_google_));

  // Verify that A=B was set as a host cookie rather than a domain
  // cookie -- should not be accessible from a sub sub-domain.
  this->MatchCookieLines("C=D",
      this->GetCookies(cs, GURL("http://foo.www.google.izzle")));

  // Test and make sure we find domain cookies on the same domain.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              "E=F; domain=.www.google.izzle"));
  this->MatchCookieLines("A=B; C=D; E=F",
                         this->GetCookies(cs, this->url_google_));

  // Test setting a domain= that doesn't start w/ a dot, should
  // treat it as a domain cookie, as if there was a pre-pended dot.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              "G=H; domain=www.google.izzle"));
  this->MatchCookieLines("A=B; C=D; E=F; G=H",
                         this->GetCookies(cs, this->url_google_));

  // Test domain enforcement, should fail on a sub-domain or something too deep.
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_, "I=J; domain=.izzle"));
  this->MatchCookieLines("", this->GetCookies(cs, GURL("http://a.izzle")));
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_,
                               "K=L; domain=.bla.www.google.izzle"));
  this->MatchCookieLines("C=D; E=F; G=H",
      this->GetCookies(cs, GURL("http://bla.www.google.izzle")));
  this->MatchCookieLines("A=B; C=D; E=F; G=H",
      this->GetCookies(cs, this->url_google_));
}

// FireFox recognizes domains containing trailing periods as valid.
// IE and Safari do not. Assert the expected policy here.
TYPED_TEST_P(CookieStoreTest, DomainWithTrailingDotTest) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_,
                               "a=1; domain=.www.google.com."));
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_,
                               "b=2; domain=.www.google.com.."));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
}

// Test that cookies can bet set on higher level domains.
// http://b/issue?id=896491
TYPED_TEST_P(CookieStoreTest, ValidSubdomainTest) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  GURL url_abcd("http://a.b.c.d.com");
  GURL url_bcd("http://b.c.d.com");
  GURL url_cd("http://c.d.com");
  GURL url_d("http://d.com");

  EXPECT_TRUE(this->SetCookie(cs, url_abcd, "a=1; domain=.a.b.c.d.com"));
  EXPECT_TRUE(this->SetCookie(cs, url_abcd, "b=2; domain=.b.c.d.com"));
  EXPECT_TRUE(this->SetCookie(cs, url_abcd, "c=3; domain=.c.d.com"));
  EXPECT_TRUE(this->SetCookie(cs, url_abcd, "d=4; domain=.d.com"));

  this->MatchCookieLines("a=1; b=2; c=3; d=4", this->GetCookies(cs, url_abcd));
  this->MatchCookieLines("b=2; c=3; d=4", this->GetCookies(cs, url_bcd));
  this->MatchCookieLines("c=3; d=4", this->GetCookies(cs, url_cd));
  this->MatchCookieLines("d=4", this->GetCookies(cs, url_d));

  // Check that the same cookie can exist on different sub-domains.
  EXPECT_TRUE(this->SetCookie(cs, url_bcd, "X=bcd; domain=.b.c.d.com"));
  EXPECT_TRUE(this->SetCookie(cs, url_bcd, "X=cd; domain=.c.d.com"));
  this->MatchCookieLines("b=2; c=3; d=4; X=bcd; X=cd",
                         this->GetCookies(cs, url_bcd));
  this->MatchCookieLines("c=3; d=4; X=cd", this->GetCookies(cs, url_cd));
}

// Test that setting a cookie which specifies an invalid domain has
// no side-effect. An invalid domain in this context is one which does
// not match the originating domain.
// http://b/issue?id=896472
TYPED_TEST_P(CookieStoreTest, InvalidDomainTest) {
  {
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url_foobar("http://foo.bar.com");

    // More specific sub-domain than allowed.
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "a=1; domain=.yo.foo.bar.com"));

    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "b=2; domain=.foo.com"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "c=3; domain=.bar.foo.com"));

    // Different TLD, but the rest is a substring.
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "d=4; domain=.foo.bar.com.net"));

    // A substring that isn't really a parent domain.
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "e=5; domain=ar.com"));

    // Completely invalid domains:
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "f=6; domain=."));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "g=7; domain=/"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "h=8; domain=http://foo.bar.com"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "i=9; domain=..foo.bar.com"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar, "j=10; domain=..bar.com"));

    // Make sure there isn't something quirky in the domain canonicalization
    // that supports full URL semantics.
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "k=11; domain=.foo.bar.com?blah"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "l=12; domain=.foo.bar.com/blah"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "m=13; domain=.foo.bar.com:80"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "n=14; domain=.foo.bar.com:"));
    EXPECT_FALSE(this->SetCookie(cs, url_foobar,
                                 "o=15; domain=.foo.bar.com#sup"));

    this->MatchCookieLines("", this->GetCookies(cs, url_foobar));
  }

  {
    // Make sure the cookie code hasn't gotten its subdomain string handling
    // reversed, missed a suffix check, etc.  It's important here that the two
    // hosts below have the same domain + registry.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url_foocom("http://foo.com.com");
    EXPECT_FALSE(this->SetCookie(cs, url_foocom,
                                 "a=1; domain=.foo.com.com.com"));
    this->MatchCookieLines("", this->GetCookies(cs, url_foocom));
  }
}

// Test the behavior of omitting dot prefix from domain, should
// function the same as FireFox.
// http://b/issue?id=889898
TYPED_TEST_P(CookieStoreTest, DomainWithoutLeadingDotTest) {
  {  // The omission of dot results in setting a domain cookie.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url_hosted("http://manage.hosted.filefront.com");
    GURL url_filefront("http://www.filefront.com");
    EXPECT_TRUE(this->SetCookie(cs, url_hosted,
                                "sawAd=1; domain=filefront.com"));
    this->MatchCookieLines("sawAd=1", this->GetCookies(cs, url_hosted));
    this->MatchCookieLines("sawAd=1", this->GetCookies(cs, url_filefront));
  }

  {  // Even when the domains match exactly, don't consider it host cookie.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://www.google.com");
    EXPECT_TRUE(this->SetCookie(cs, url, "a=1; domain=www.google.com"));
    this->MatchCookieLines("a=1", this->GetCookies(cs, url));
    this->MatchCookieLines("a=1",
        this->GetCookies(cs, GURL("http://sub.www.google.com")));
    this->MatchCookieLines("",
        this->GetCookies(cs, GURL("http://something-else.com")));
  }
}

// Test that the domain specified in cookie string is treated case-insensitive
// http://b/issue?id=896475.
TYPED_TEST_P(CookieStoreTest, CaseInsensitiveDomainTest) {
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
  GURL url("http://www.google.com");
  EXPECT_TRUE(this->SetCookie(cs, url, "a=1; domain=.GOOGLE.COM"));
  EXPECT_TRUE(this->SetCookie(cs, url, "b=2; domain=.wWw.gOOgLE.coM"));
  this->MatchCookieLines("a=1; b=2", this->GetCookies(cs, url));
}

TYPED_TEST_P(CookieStoreTest, TestIpAddress) {
  GURL url_ip("http://1.2.3.4/weee");
  {
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    EXPECT_TRUE(this->SetCookie(cs, url_ip, kValidCookieLine));
    this->MatchCookieLines("A=B", this->GetCookies(cs, url_ip));
  }

  {  // IP addresses should not be able to set domain cookies.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    EXPECT_FALSE(this->SetCookie(cs, url_ip, "b=2; domain=.1.2.3.4"));
    EXPECT_FALSE(this->SetCookie(cs, url_ip, "c=3; domain=.3.4"));
    this->MatchCookieLines("", this->GetCookies(cs, url_ip));
    // It should be allowed to set a cookie if domain= matches the IP address
    // exactly.  This matches IE/Firefox, even though it seems a bit wrong.
    EXPECT_FALSE(this->SetCookie(cs, url_ip, "b=2; domain=1.2.3.3"));
    this->MatchCookieLines("", this->GetCookies(cs, url_ip));
    EXPECT_TRUE(this->SetCookie(cs, url_ip, "b=2; domain=1.2.3.4"));
    this->MatchCookieLines("b=2", this->GetCookies(cs, url_ip));
  }
}

// Test host cookies, and setting of cookies on TLD.
TYPED_TEST_P(CookieStoreTest, TestNonDottedAndTLD) {
  {
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://com/");
    // Allow setting on "com", (but only as a host cookie).
    EXPECT_TRUE(this->SetCookie(cs, url, "a=1"));
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=.com"));
    EXPECT_FALSE(this->SetCookie(cs, url, "c=3; domain=com"));
    this->MatchCookieLines("a=1", this->GetCookies(cs, url));
    // Make sure it doesn't show up for a normal .com, it should be a host
    // not a domain cookie.
    this->MatchCookieLines("",
        this->GetCookies(cs, GURL("http://hopefully-no-cookies.com/")));
    if (TypeParam::supports_non_dotted_domains) {
      this->MatchCookieLines("", this->GetCookies(cs, GURL("http://.com/")));
    }
  }

  {
    // http://com. should be treated the same as http://com.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://com./index.html");
    if (TypeParam::supports_trailing_dots) {
      EXPECT_TRUE(this->SetCookie(cs, url, "a=1"));
      this->MatchCookieLines("a=1", this->GetCookies(cs, url));
      this->MatchCookieLines("",
          this->GetCookies(cs, GURL("http://hopefully-no-cookies.com./")));
    } else {
      EXPECT_FALSE(this->SetCookie(cs, url, "a=1"));
    }
  }

  {  // Should not be able to set host cookie from a subdomain.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://a.b");
    EXPECT_FALSE(this->SetCookie(cs, url, "a=1; domain=.b"));
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=b"));
    this->MatchCookieLines("", this->GetCookies(cs, url));
  }

  {  // Same test as above, but explicitly on a known TLD (com).
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://google.com");
    EXPECT_FALSE(this->SetCookie(cs, url, "a=1; domain=.com"));
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=com"));
    this->MatchCookieLines("", this->GetCookies(cs, url));
  }

  {  // Make sure can't set cookie on TLD which is dotted.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://google.co.uk");
    EXPECT_FALSE(this->SetCookie(cs, url, "a=1; domain=.co.uk"));
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=.uk"));
    this->MatchCookieLines("", this->GetCookies(cs, url));
    this->MatchCookieLines("",
        this->GetCookies(cs, GURL("http://something-else.co.uk")));
    this->MatchCookieLines("",
        this->GetCookies(cs, GURL("http://something-else.uk")));
  }

  {  // Intranet URLs should only be able to set host cookies.
    scoped_refptr<CookieStore> cs(this->GetCookieStore());
    GURL url("http://b");
    EXPECT_TRUE(this->SetCookie(cs, url, "a=1"));
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=.b"));
    EXPECT_FALSE(this->SetCookie(cs, url, "c=3; domain=b"));
    this->MatchCookieLines("a=1", this->GetCookies(cs, url));
  }
}

// Test reading/writing cookies when the domain ends with a period,
// as in "www.google.com."
TYPED_TEST_P(CookieStoreTest, TestHostEndsWithDot) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  GURL url("http://www.google.com");
  GURL url_with_dot("http://www.google.com.");
  EXPECT_TRUE(this->SetCookie(cs, url, "a=1"));
  this->MatchCookieLines("a=1", this->GetCookies(cs, url));

  if (TypeParam::supports_trailing_dots) {
    // Do not share cookie space with the dot version of domain.
    // Note: this is not what FireFox does, but it _is_ what IE+Safari do.
    EXPECT_FALSE(this->SetCookie(cs, url, "b=2; domain=.www.google.com."));
    this->MatchCookieLines("a=1", this->GetCookies(cs, url));

    EXPECT_TRUE(this->SetCookie(cs, url_with_dot, "b=2; domain=.google.com."));
    this->MatchCookieLines("b=2", this->GetCookies(cs, url_with_dot));
  } else {
    EXPECT_TRUE(this->SetCookie(cs, url, "b=2; domain=.www.google.com."));
    EXPECT_FALSE(this->SetCookie(cs, url_with_dot, "b=2; domain=.google.com."));
  }

  // Make sure there weren't any side effects.
  this->MatchCookieLines("",
      this->GetCookies(cs, GURL("http://hopefully-no-cookies.com/")));
  this->MatchCookieLines("", this->GetCookies(cs, GURL("http://.com/")));
}

TYPED_TEST_P(CookieStoreTest, InvalidScheme) {
  if (!TypeParam::filters_schemes)
    return;

  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_FALSE(this->SetCookie(cs, GURL(kUrlFtp), kValidCookieLine));
}

TYPED_TEST_P(CookieStoreTest, InvalidScheme_Read) {
  if (!TypeParam::filters_schemes)
    return;

  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_TRUE(this->SetCookie(cs, GURL(kUrlGoogle), kValidDomainCookieLine));
  this->MatchCookieLines("", this->GetCookies(cs, GURL(kUrlFtp)));
}

TYPED_TEST_P(CookieStoreTest, PathTest) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  std::string url("http://www.google.izzle");
  EXPECT_TRUE(this->SetCookie(cs, GURL(url), "A=B; path=/wee"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, GURL(url + "/wee")));
  this->MatchCookieLines("A=B", this->GetCookies(cs, GURL(url + "/wee/")));
  this->MatchCookieLines("A=B", this->GetCookies(cs, GURL(url + "/wee/war")));
  this->MatchCookieLines("A=B",
      this->GetCookies(cs, GURL(url + "/wee/war/more/more")));
  if (!TypeParam::has_path_prefix_bug)
    this->MatchCookieLines("", this->GetCookies(cs, GURL(url + "/weehee")));
  this->MatchCookieLines("", this->GetCookies(cs, GURL(url + "/")));

  // If we add a 0 length path, it should default to /
  EXPECT_TRUE(this->SetCookie(cs, GURL(url), "A=C; path="));
  this->MatchCookieLines("A=B; A=C", this->GetCookies(cs, GURL(url + "/wee")));
  this->MatchCookieLines("A=C", this->GetCookies(cs, GURL(url + "/")));
}

TYPED_TEST_P(CookieStoreTest, HttpOnlyTest) {
  if (!TypeParam::supports_http_only)
    return;

  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  options.set_include_httponly();

  // Create a httponly cookie.
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=B; httponly",
                                         options));

  // Check httponly read protection.
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("A=B",
      this->GetCookiesWithOptions(cs, this->url_google_, options));

  // Check httponly overwrite protection.
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_, "A=C"));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("A=B",
      this->GetCookiesWithOptions(cs, this->url_google_, options));
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=C",
                                         options));
  this->MatchCookieLines("A=C", this->GetCookies(cs, this->url_google_));

  // Check httponly create protection.
  EXPECT_FALSE(this->SetCookie(cs, this->url_google_, "B=A; httponly"));
  this->MatchCookieLines("A=C",
      this->GetCookiesWithOptions(cs, this->url_google_, options));
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "B=A; httponly",
                                         options));
  this->MatchCookieLines("A=C; B=A",
      this->GetCookiesWithOptions(cs, this->url_google_, options));
  this->MatchCookieLines("A=C", this->GetCookies(cs, this->url_google_));
}

TYPED_TEST_P(CookieStoreTest, TestGetCookiesWithInfo) {
  if (!TypeParam::supports_cookies_with_info)
    return;

  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;

  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=B",
                                         options));
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_,
    "C=D; Mac-Key=390jfn0awf3; Mac-Algorithm=hmac-sha-1", options));

  this->MatchCookieLines("A=B; C=D",
      this->GetCookiesWithOptions(cs, this->url_google_, options));

  std::string cookie_line;
  std::vector<CookieStore::CookieInfo> cookie_infos;

  this->GetCookiesWithInfo(cs, this->url_google_, options, &cookie_line,
                           &cookie_infos);

  EXPECT_EQ("A=B; C=D", cookie_line);

  EXPECT_EQ(2U, cookie_infos.size());

  EXPECT_EQ("A", cookie_infos[0].name);
  EXPECT_EQ("", cookie_infos[0].mac_key);
  EXPECT_EQ("", cookie_infos[0].mac_algorithm);

  EXPECT_EQ("C", cookie_infos[1].name);
  EXPECT_EQ("390jfn0awf3", cookie_infos[1].mac_key);
  EXPECT_EQ("hmac-sha-1", cookie_infos[1].mac_algorithm);
}

TYPED_TEST_P(CookieStoreTest, TestCookieDeletion) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());

  // Create a session cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, kValidCookieLine));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  // Delete it via Max-Age.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) + "; max-age=0"));
  this->MatchCookieLineWithTimeout(cs, this->url_google_, "");

  // Create a session cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, kValidCookieLine));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  // Delete it via Expires.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));

  // Create a persistent cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Mon, 18-Apr-22 22:50:13 GMT"));

  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  // Delete it via Max-Age.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) + "; max-age=0"));
  this->MatchCookieLineWithTimeout(cs, this->url_google_, "");

  // Create a persistent cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  // Delete it via Expires.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));

  // Create a persistent cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  // Delete it via Expires, with a unix epoch of 0.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_,
                              std::string(kValidCookieLine) +
                              "; expires=Thu, 1-Jan-1970 00:00:00 GMT"));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
}

TYPED_TEST_P(CookieStoreTest, TestDeleteAllCreatedBetween) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  const base::Time last_month = base::Time::Now() -
                                base::TimeDelta::FromDays(30);
  const base::Time last_minute = base::Time::Now() -
                                 base::TimeDelta::FromMinutes(1);
  const base::Time next_minute = base::Time::Now() +
                                 base::TimeDelta::FromMinutes(1);
  const base::Time next_month = base::Time::Now() +
                                base::TimeDelta::FromDays(30);

  // Add a cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
   // Check that the cookie is in the store.
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));

  // Remove cookies in empty intervals.
  EXPECT_EQ(0, this->DeleteCreatedBetween(cs, last_month, last_minute));
  EXPECT_EQ(0, this->DeleteCreatedBetween(cs, next_minute, next_month));
  // Check that the cookie is still there.
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));

  // Remove the cookie with an interval defined by two dates.
  EXPECT_EQ(1, this->DeleteCreatedBetween(cs, last_minute, next_minute));
  // Check that the cookie disappeared.
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));

  // Add another cookie.
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "C=D"));
  // Check that the cookie is in the store.
  this->MatchCookieLines("C=D", this->GetCookies(cs, this->url_google_));

  // Remove the cookie with a null ending time.
  EXPECT_EQ(1, this->DeleteCreatedBetween(cs, last_minute, base::Time()));
  // Check that the cookie disappeared.
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
}

TYPED_TEST_P(CookieStoreTest, TestSecure) {
    scoped_refptr<CookieStore> cs(this->GetCookieStore());

  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_secure_));

  EXPECT_TRUE(this->SetCookie(cs, this->url_google_secure_, "A=B; secure"));
  // The secure should overwrite the non-secure.
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_secure_));

  EXPECT_TRUE(this->SetCookie(cs, this->url_google_secure_, "D=E; secure"));
  this->MatchCookieLines("", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("A=B; D=E",
                         this->GetCookies(cs, this->url_google_secure_));

  EXPECT_TRUE(this->SetCookie(cs, this->url_google_secure_, "A=B"));
  // The non-secure should overwrite the secure.
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  this->MatchCookieLines("D=E; A=B",
                         this->GetCookies(cs, this->url_google_secure_));
}

static const int kLastAccessThresholdMilliseconds = 200;

// Formerly NetUtilTest.CookieTest back when we used wininet's cookie handling.
TYPED_TEST_P(CookieStoreTest, NetUtilCookieTest) {
  const GURL test_url("http://mojo.jojo.google.izzle/");

  scoped_refptr<CookieStore> cs(this->GetCookieStore());

  EXPECT_TRUE(this->SetCookie(cs, test_url, "foo=bar"));
  std::string value = this->GetCookies(cs, test_url);
  this->MatchCookieLines("foo=bar", value);

  // test that we can retrieve all cookies:
  EXPECT_TRUE(this->SetCookie(cs, test_url, "x=1"));
  EXPECT_TRUE(this->SetCookie(cs, test_url, "y=2"));

  std::string result = this->GetCookies(cs, test_url);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("x=1"), std::string::npos) << result;
  EXPECT_NE(result.find("y=2"), std::string::npos) << result;
}

TYPED_TEST_P(CookieStoreTest, OverwritePersistentCookie) {
  GURL url_google("http://www.google.com/");
  GURL url_chromium("http://chromium.org");
  scoped_refptr<CookieStore> cs(this->GetCookieStore());

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(
      this->SetCookie(cs, url_google, "a=val1; path=/path1; "
                      "expires=Mon, 18-Apr-22 22:50:13 GMT"));

  // Insert a cookie "b" for path "/path1"
  EXPECT_TRUE(
      this->SetCookie(cs, url_google, "b=val1; path=/path1; "
                      "expires=Mon, 18-Apr-22 22:50:14 GMT"));

  // Insert a cookie "b" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(
    this->SetCookieWithOptions(cs, url_google,
                               "b=val2; path=/path1; httponly; "
                               "expires=Mon, 18-Apr-22 22:50:14 GMT",
                               allow_httponly));

  // Insert a cookie "a" for path "/path1". This should overwrite.
  EXPECT_TRUE(this->SetCookie(cs, url_google,
                              "a=val33; path=/path1; "
                              "expires=Mon, 18-Apr-22 22:50:14 GMT"));

  // Insert a cookie "a" for path "/path2". This should NOT overwrite
  // cookie "a", since the path is different.
  EXPECT_TRUE(this->SetCookie(cs, url_google,
                              "a=val9; path=/path2; "
                              "expires=Mon, 18-Apr-22 22:50:14 GMT"));

  // Insert a cookie "a" for path "/path1", but this time for "chromium.org".
  // Although the name and path match, the hostnames do not, so shouldn't
  // overwrite.
  EXPECT_TRUE(this->SetCookie(cs, url_chromium,
                              "a=val99; path=/path1; "
                              "expires=Mon, 18-Apr-22 22:50:14 GMT"));

  if (TypeParam::supports_http_only) {
    this->MatchCookieLines("a=val33",
        this->GetCookies(cs, GURL("http://www.google.com/path1")));
  } else {
    this->MatchCookieLines("a=val33; b=val2",
        this->GetCookies(cs, GURL("http://www.google.com/path1")));
  }
  this->MatchCookieLines("a=val9",
      this->GetCookies(cs, GURL("http://www.google.com/path2")));
  this->MatchCookieLines("a=val99",
      this->GetCookies(cs, GURL("http://chromium.org/path1")));
}

TYPED_TEST_P(CookieStoreTest, CookieOrdering) {
  // Put a random set of cookies into a store and make sure they're returned in
  // the right order.
  // Cookies should be sorted by path length and creation time, as per RFC6265.
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_TRUE(this->SetCookie(cs, GURL("http://d.c.b.a.google.com/aa/x.html"),
                              "c=1"));
  EXPECT_TRUE(this->SetCookie(cs, GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                              "d=1; domain=b.a.google.com"));
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      TypeParam::creation_time_granularity_in_ms));
  EXPECT_TRUE(this->SetCookie(cs, GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                              "a=4; domain=b.a.google.com"));
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
      TypeParam::creation_time_granularity_in_ms));
  EXPECT_TRUE(this->SetCookie(cs,
                              GURL("http://c.b.a.google.com/aa/bb/cc/x.html"),
                              "e=1; domain=c.b.a.google.com"));
  EXPECT_TRUE(this->SetCookie(cs,
                              GURL("http://d.c.b.a.google.com/aa/bb/x.html"),
                              "b=1"));
  EXPECT_TRUE(this->SetCookie(cs, GURL("http://news.bbc.co.uk/midpath/x.html"),
                              "g=10"));
  EXPECT_EQ("d=1; a=4; e=1; b=1; c=1",
      this->GetCookies(cs, GURL("http://d.c.b.a.google.com/aa/bb/cc/dd")));
}

REGISTER_TYPED_TEST_CASE_P(CookieStoreTest,
    TypeTest, DomainTest, DomainWithTrailingDotTest, ValidSubdomainTest,
    InvalidDomainTest, DomainWithoutLeadingDotTest, CaseInsensitiveDomainTest,
    TestIpAddress, TestNonDottedAndTLD, TestHostEndsWithDot, InvalidScheme,
    InvalidScheme_Read, PathTest, HttpOnlyTest, TestGetCookiesWithInfo,
    TestCookieDeletion, TestDeleteAllCreatedBetween, TestSecure,
    NetUtilCookieTest, OverwritePersistentCookie, CookieOrdering);

template<class CookieStoreTestTraits>
class MultiThreadedCookieStoreTest :
    public CookieStoreTest<CookieStoreTestTraits> {
 public:
  MultiThreadedCookieStoreTest() : other_thread_("CMTthread") {}

  // Helper methods for calling the asynchronous CookieStore methods
  // from a different thread.

  void GetCookiesTask(CookieStore* cs,
                      const GURL& url,
                      GetCookieStringCallback* callback) {
    CookieOptions options;
    if (!CookieStoreTestTraits::supports_http_only)
      options.set_include_httponly();
    cs->GetCookiesWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieStringCallback::Run, base::Unretained(callback)));
  }

  void GetCookiesWithOptionsTask(CookieStore* cs,
                                 const GURL& url,
                                 const CookieOptions& options,
                                 GetCookieStringCallback* callback) {
    cs->GetCookiesWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieStringCallback::Run, base::Unretained(callback)));
  }

  void GetCookiesWithInfoTask(CookieStore* cs,
                              const GURL& url,
                              const CookieOptions& options,
                              GetCookiesWithInfoCallback* callback) {
    cs->GetCookiesWithInfoAsync(
        url, options,
        base::Bind(&GetCookiesWithInfoCallback::Run,
                   base::Unretained(callback)));
  }

  void SetCookieWithOptionsTask(CookieStore* cs,
                                const GURL& url,
                                const std::string& cookie_line,
                                const CookieOptions& options,
                                SetCookieCallback* callback) {
    cs->SetCookieWithOptionsAsync(
        url, cookie_line, options,
        base::Bind(&SetCookieCallback::Run, base::Unretained(callback)));
  }

  void DeleteCookieTask(CookieStore* cs,
                        const GURL& url,
                        const std::string& cookie_name,
                        DeleteCookieCallback* callback) {
    cs->DeleteCookieAsync(
        url, cookie_name,
        base::Bind(&DeleteCookieCallback::Run, base::Unretained(callback)));
  }

    void DeleteSessionCookiesTask(CookieStore* cs, DeleteCallback* callback) {
    cs->DeleteSessionCookiesAsync(
        base::Bind(&DeleteCallback::Run, base::Unretained(callback)));
  }

 protected:
  void RunOnOtherThread(const base::Closure& task) {
    other_thread_.Start();
    other_thread_.message_loop()->PostTask(FROM_HERE, task);
    CookieStoreTest<CookieStoreTestTraits>::RunFor(kTimeout);
    other_thread_.Stop();
  }

  Thread other_thread_;
};

TYPED_TEST_CASE_P(MultiThreadedCookieStoreTest);

// TODO(ycxiao): Eventually, we will need to create a separate thread, create
// the cookie store on that thread (or at least its store, i.e., the DB
// thread).
TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckGetCookies) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
  this->MatchCookieLines("A=B", this->GetCookies(cs, this->url_google_));
  GetCookieStringCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::GetCookiesTask,
      base::Unretained(this),
      cs, this->url_google_, &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("A=B", callback.cookie());
}

TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckGetCookiesWithOptions) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  if (!TypeParam::supports_http_only)
    options.set_include_httponly();
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
  this->MatchCookieLines("A=B",
      this->GetCookiesWithOptions(cs, this->url_google_, options));
  GetCookieStringCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::GetCookiesWithOptionsTask,
      base::Unretained(this),
      cs, this->url_google_, options, &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("A=B", callback.cookie());
}

TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckGetCookiesWithInfo) {
  if (!TypeParam::supports_cookies_with_info)
    return;
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  std::string cookie_line;
  std::vector<CookieStore::CookieInfo> cookie_infos;
  EXPECT_TRUE(this->SetCookie(cs, this->url_google_, "A=B"));
  this->GetCookiesWithInfo(cs, this->url_google_, options, &cookie_line,
                           &cookie_infos);
  this->MatchCookieLines("A=B", cookie_line);
  EXPECT_EQ(1U, cookie_infos.size());
  EXPECT_EQ("A", cookie_infos[0].name);
  EXPECT_EQ("", cookie_infos[0].mac_key);
  EXPECT_EQ("", cookie_infos[0].mac_algorithm);
  GetCookiesWithInfoCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::GetCookiesWithInfoTask,
      base::Unretained(this), cs, this->url_google_, options, &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  this->MatchCookieLines("A=B", callback.cookie_line());
  EXPECT_EQ(1U, callback.cookie_info().size());
  EXPECT_EQ("A", callback.cookie_info()[0].name);
  EXPECT_EQ("", callback.cookie_info()[0].mac_key);
  EXPECT_EQ("", callback.cookie_info()[0].mac_algorithm);
}

TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckSetCookieWithOptions) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  if (!TypeParam::supports_http_only)
    options.set_include_httponly();
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=B",
                                         options));
  SetCookieCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::SetCookieWithOptionsTask,
      base::Unretained(this),
      cs, this->url_google_, "A=B", options, &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckDeleteCookie) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  if (!TypeParam::supports_http_only)
    options.set_include_httponly();
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=B",
                                         options));
  this->DeleteCookie(cs, this->url_google_, "A");
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_, "A=B",
                                         options));
  DeleteCookieCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::DeleteCookieTask,
      base::Unretained(this),
      cs, this->url_google_, "A", &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
}

TYPED_TEST_P(MultiThreadedCookieStoreTest, ThreadCheckDeleteSessionCookies) {
  scoped_refptr<CookieStore> cs(this->GetCookieStore());
  CookieOptions options;
  if (!TypeParam::supports_http_only)
    options.set_include_httponly();
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_,
                                         "A=B", options));
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_,
      "B=C; expires=Mon, 18-Apr-22 22:50:13 GMT", options));
  EXPECT_EQ(1, this->DeleteSessionCookies(cs));
  EXPECT_EQ(0, this->DeleteSessionCookies(cs));

  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->url_google_,
                                         "A=B", options));
  DeleteCallback callback(&this->other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieStoreTest<TypeParam>::DeleteSessionCookiesTask,
      base::Unretained(this),
      cs, &callback);
  this->RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1, callback.num_deleted());
}

REGISTER_TYPED_TEST_CASE_P(MultiThreadedCookieStoreTest,
    ThreadCheckGetCookies, ThreadCheckGetCookiesWithOptions,
    ThreadCheckGetCookiesWithInfo, ThreadCheckSetCookieWithOptions,
    ThreadCheckDeleteCookie, ThreadCheckDeleteSessionCookies);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_UNITTEST_H_
