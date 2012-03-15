// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_monster_store_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class CookieMonsterTest : public testing::Test {
 public:
  CookieMonsterTest() : message_loop_(new MessageLoopForIO()) {}

 private:
  scoped_ptr<MessageLoop> message_loop_;
};
}

static const int kNumCookies = 20000;
static const char kCookieLine[] = "A  = \"b=;\\\"\"  ;secure;;;";

namespace net {

TEST(ParsedCookieTest, TestParseCookies) {
  std::string cookie(kCookieLine);
  PerfTimeLogger timer("Parsed_cookie_parse_cookies");
  for (int i = 0; i < kNumCookies; ++i) {
    CookieMonster::ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  timer.Done();
}

TEST(ParsedCookieTest, TestParseBigCookies) {
  std::string cookie(3800, 'z');
  cookie += kCookieLine;
  PerfTimeLogger timer("Parsed_cookie_parse_big_cookies");
  for (int i = 0; i < kNumCookies; ++i) {
    CookieMonster::ParsedCookie pc(cookie);
    EXPECT_TRUE(pc.IsValid());
  }
  timer.Done();
}

static const GURL kUrlGoogle("http://www.google.izzle");

class BaseCallback {
 public:
  BaseCallback() : has_run_(false) {}

 protected:
  void WaitForCallback() {
    // Note that the performance tests currently all operate on a loaded cookie
    // store (or, more precisely, one that has no backing persistent store).
    // Therefore, callbacks will actually always complete synchronously. If the
    // tests get more advanced we need to add other means of signaling
    // completion.
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(has_run_);
    has_run_ = false;
  }

  void Run() {
    has_run_ = true;
  }

  bool has_run_;
};


class SetCookieCallback  : public BaseCallback {
 public:
  void SetCookie(
      CookieMonster* cm, const GURL& gurl, const std::string& cookie) {
    cm->SetCookieWithOptionsAsync(gurl, cookie, options_, base::Bind(
        &SetCookieCallback::Run, base::Unretained(this)));
    WaitForCallback();
  }
 private:
  void Run(bool success) {
    EXPECT_TRUE(success);
    BaseCallback::Run();
  }
  net::CookieOptions options_;
};

class GetCookiesCallback : public BaseCallback {
 public:
  const std::string& GetCookies(CookieMonster* cm, const GURL& gurl) {
    cm->GetCookiesWithOptionsAsync(gurl, options_, base::Bind(
        &GetCookiesCallback::Run, base::Unretained(this)));
    WaitForCallback();
    return cookies_;
  }

 private:
  void Run(const std::string& cookies) {
    cookies_ = cookies;
    BaseCallback::Run();
  }
  std::string cookies_;
  net::CookieOptions options_;
};

class GetCookiesWithInfoCallback : public BaseCallback {
 public:
  const std::string& GetCookiesWithInfo(CookieMonster* cm, const GURL& gurl) {
    cm->GetCookiesWithInfoAsync(gurl, options_, base::Bind(
        &GetCookiesWithInfoCallback::Run,
        base::Unretained(this)));
    WaitForCallback();
    return cookies_;
  }

 private:
  void Run(
    const std::string& cookie_line,
    const std::vector<CookieStore::CookieInfo>& cookie_infos) {
    cookies_ = cookie_line;
    BaseCallback::Run();
  }

  std::string cookies_;
  net::CookieOptions options_;
};


TEST_F(CookieMonsterTest, TestAddCookiesOnSingleHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  std::vector<std::string> cookies;
  for (int i = 0; i < kNumCookies; i++) {
    cookies.push_back(base::StringPrintf("a%03d=b", i));
  }

  SetCookieCallback setCookieCallback;

  // Add a bunch of cookies on a single host
  PerfTimeLogger timer("Cookie_monster_add_single_host");

  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    setCookieCallback.SetCookie(cm, kUrlGoogle, *it);
  }
  timer.Done();

  GetCookiesCallback getCookiesCallback;

  PerfTimeLogger timer2("Cookie_monster_query_single_host");
  for (std::vector<std::string>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    getCookiesCallback.GetCookies(cm, kUrlGoogle);
  }
  timer2.Done();

  PerfTimeLogger timer3("Cookie_monster_deleteall_single_host");
  cm->DeleteAllAsync(CookieMonster::DeleteCallback());
  MessageLoop::current()->RunAllPending();
  timer3.Done();
}

TEST_F(CookieMonsterTest, TestAddCookieOnManyHosts) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  std::string cookie(kCookieLine);
  std::vector<GURL> gurls;  // just wanna have ffffuunnn
  for (int i = 0; i < kNumCookies; ++i) {
    gurls.push_back(GURL(base::StringPrintf("https://a%04d.izzle", i)));
  }

  SetCookieCallback setCookieCallback;

  // Add a cookie on a bunch of host
  PerfTimeLogger timer("Cookie_monster_add_many_hosts");
  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    setCookieCallback.SetCookie(cm, *it, cookie);
  }
  timer.Done();

  GetCookiesCallback getCookiesCallback;

  PerfTimeLogger timer2("Cookie_monster_query_many_hosts");
  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    getCookiesCallback.GetCookies(cm, *it);
  }
  timer2.Done();

  PerfTimeLogger timer3("Cookie_monster_deleteall_many_hosts");
  cm->DeleteAllAsync(CookieMonster::DeleteCallback());
  MessageLoop::current()->RunAllPending();
  timer3.Done();
}

TEST_F(CookieMonsterTest, TestGetCookiesWithInfo) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  std::vector<GURL> gurls;
  for (int i = 0; i < kNumCookies; ++i)
    gurls.push_back(GURL(base::StringPrintf("https://a%04d.izzle", i)));

  SetCookieCallback setCookieCallback;

  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    setCookieCallback.SetCookie(cm, *it, kCookieLine);
  }

  GetCookiesWithInfoCallback getCookiesCallback;

  PerfTimeLogger timer("Cookie_monster_get_cookie_info");
  for (std::vector<GURL>::const_iterator it = gurls.begin();
       it != gurls.end(); ++it) {
    getCookiesCallback.GetCookiesWithInfo(cm, *it);
  }
  timer.Done();
}

static int CountInString(const std::string& str, char c) {
  return std::count(str.begin(), str.end(), c);
}

TEST_F(CookieMonsterTest, TestDomainTree) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  GetCookiesCallback getCookiesCallback;
  SetCookieCallback setCookieCallback;
  const char* domain_cookie_format_tree = "a=b; domain=%s";
  const std::string domain_base("top.com");

  std::vector<std::string> domain_list;

  // Create a balanced binary tree of domains on which the cookie is set.
  domain_list.push_back(domain_base);
  for (int i1 = 0; i1 < 2; i1++) {
    std::string domain_base_1((i1 ? "a." : "b.") + domain_base);
    EXPECT_EQ("top.com", cm->GetKey(domain_base_1));
    domain_list.push_back(domain_base_1);
    for (int i2 = 0; i2 < 2; i2++) {
      std::string domain_base_2((i2 ? "a." : "b.") + domain_base_1);
      EXPECT_EQ("top.com", cm->GetKey(domain_base_2));
      domain_list.push_back(domain_base_2);
      for (int i3 = 0; i3 < 2; i3++) {
        std::string domain_base_3((i3 ? "a." : "b.") + domain_base_2);
        EXPECT_EQ("top.com", cm->GetKey(domain_base_3));
        domain_list.push_back(domain_base_3);
        for (int i4 = 0; i4 < 2; i4++) {
          std::string domain_base_4((i4 ? "a." : "b.") + domain_base_3);
          EXPECT_EQ("top.com", cm->GetKey(domain_base_4));
          domain_list.push_back(domain_base_4);
        }
      }
    }
  }


  EXPECT_EQ(31u, domain_list.size());
  for (std::vector<std::string>::const_iterator it = domain_list.begin();
       it != domain_list.end(); it++) {
    GURL gurl("https://" + *it + "/");
    const std::string cookie = base::StringPrintf(domain_cookie_format_tree,
                                                  it->c_str());
    setCookieCallback.SetCookie(cm, gurl, cookie);
  }
  EXPECT_EQ(31u, cm->GetAllCookies().size());

  GURL probe_gurl("https://b.a.b.a.top.com/");
  std::string cookie_line = getCookiesCallback.GetCookies(cm, probe_gurl);
  EXPECT_EQ(5, CountInString(cookie_line, '=')) << "Cookie line: " <<
      cookie_line;
  PerfTimeLogger timer("Cookie_monster_query_domain_tree");
  for (int i = 0; i < kNumCookies; i++) {
    getCookiesCallback.GetCookies(cm, probe_gurl);
  }
  timer.Done();
}

TEST_F(CookieMonsterTest, TestDomainLine) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  SetCookieCallback setCookieCallback;
  GetCookiesCallback getCookiesCallback;
  std::vector<std::string> domain_list;
  GURL probe_gurl("https://b.a.b.a.top.com/");
  std::string cookie_line;

  // Create a line of 32 domain cookies such that all cookies stored
  // by effective TLD+1 will apply to probe GURL.
  // (TLD + 1 is the level above .com/org/net/etc, e.g. "top.com"
  // or "google.com".  "Effective" is added to include sites like
  // bbc.co.uk, where the effetive TLD+1 is more than one level
  // below the top level.)
  domain_list.push_back("a.top.com");
  domain_list.push_back("b.a.top.com");
  domain_list.push_back("a.b.a.top.com");
  domain_list.push_back("b.a.b.a.top.com");
  EXPECT_EQ(4u, domain_list.size());

  const char* domain_cookie_format_line = "a%03d=b; domain=%s";
  for (int i = 0; i < 8; i++) {
    for (std::vector<std::string>::const_iterator it = domain_list.begin();
         it != domain_list.end(); it++) {
      GURL gurl("https://" + *it + "/");
      const std::string cookie = base::StringPrintf(domain_cookie_format_line,
                                                    i, it->c_str());
      setCookieCallback.SetCookie(cm, gurl, cookie);
    }
  }

  cookie_line = getCookiesCallback.GetCookies(cm, probe_gurl);
  EXPECT_EQ(32, CountInString(cookie_line, '='));
  PerfTimeLogger timer2("Cookie_monster_query_domain_line");
  for (int i = 0; i < kNumCookies; i++) {
    getCookiesCallback.GetCookies(cm, probe_gurl);
  }
  timer2.Done();
}

TEST_F(CookieMonsterTest, TestImport) {
  scoped_refptr<MockPersistentCookieStore> store(new MockPersistentCookieStore);
  std::vector<CookieMonster::CanonicalCookie*> initial_cookies;
  GetCookiesCallback getCookiesCallback;

  // We want to setup a fairly large backing store, with 300 domains of 50
  // cookies each.  Creation times must be unique.
  int64 time_tick(base::Time::Now().ToInternalValue());

  for (int domain_num = 0; domain_num < 300; domain_num++) {
    std::string domain_name(base::StringPrintf(".Domain_%d.com", domain_num));
    std::string gurl("www" + domain_name);
    for (int cookie_num = 0; cookie_num < 50; cookie_num++) {
      std::string cookie_line(base::StringPrintf("Cookie_%d=1; Path=/",
                                                 cookie_num));
      AddCookieToList(gurl, cookie_line,
                      base::Time::FromInternalValue(time_tick++),
                      &initial_cookies);
    }
  }

  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  // Import will happen on first access.
  GURL gurl("www.google.com");
  CookieOptions options;
  PerfTimeLogger timer("Cookie_monster_import_from_store");
  getCookiesCallback.GetCookies(cm, gurl);
  timer.Done();

  // Just confirm keys were set as expected.
  EXPECT_EQ("domain_1.com", cm->GetKey("www.Domain_1.com"));
}

TEST_F(CookieMonsterTest, TestGetKey) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  PerfTimeLogger timer("Cookie_monster_get_key");
  for (int i = 0; i < kNumCookies; i++)
    cm->GetKey("www.google.com");
  timer.Done();
}

// This test is probing for whether garbage collection happens when it
// shouldn't.  This will not in general be visible functionally, since
// if GC runs twice in a row without any change to the store, the second
// GC run will not do anything the first one didn't.  That's why this is
// a performance test.  The test should be considered to pass if all the
// times reported are approximately the same--this indicates that no GC
// happened repeatedly for any case.
TEST_F(CookieMonsterTest, TestGCTimes) {
  SetCookieCallback setCookieCallback;

  const struct TestCase {
    const char* name;
    int num_cookies;
    int num_old_cookies;
  } test_cases[] = {
    {
      // A whole lot of recent cookies; gc shouldn't happen.
      "all_recent",
      CookieMonster::kMaxCookies * 2,
      0,
    }, {
      // Some old cookies, but still overflowing max.
      "mostly_recent",
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies / 2,
    }, {
      // Old cookies enough to bring us right down to our purge line.
      "balanced",
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies + 1,
    }, {
      "mostly_old",
      // Old cookies enough to bring below our purge line (which we
      // shouldn't do).
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 3 / 4,
    }, {
      "less_than_gc_thresh",
      // Few enough cookies that gc shouldn't happen at all.
      CookieMonster::kMaxCookies - 5,
      0,
    },
  };
  for (int ci = 0; ci < static_cast<int>(ARRAYSIZE_UNSAFE(test_cases)); ++ci) {
    const TestCase& test_case(test_cases[ci]);
    scoped_refptr<CookieMonster> cm(
        CreateMonsterFromStoreForGC(
            test_case.num_cookies, test_case.num_old_cookies,
            CookieMonster::kSafeFromGlobalPurgeDays * 2));

    GURL gurl("http://google.com");
    std::string cookie_line("z=3");
    // Trigger the Garbage collection we're allowed.
    setCookieCallback.SetCookie(cm, gurl, cookie_line);

    PerfTimeLogger timer((std::string("GC_") + test_case.name).c_str());
    for (int i = 0; i < kNumCookies; i++)
      setCookieCallback.SetCookie(cm, gurl, cookie_line);
    timer.Done();
  }
}

}  // namespace
