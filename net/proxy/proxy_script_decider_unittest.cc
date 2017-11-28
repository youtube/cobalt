// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_script_decider.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

enum Error {
  kFailedDownloading = -100,
  kFailedParsing = ERR_PAC_SCRIPT_FAILED,
};

class Rules {
 public:
  struct Rule {
    Rule(const GURL& url, int fetch_error, bool is_valid_script)
        : url(url),
          fetch_error(fetch_error),
          is_valid_script(is_valid_script) {
    }

    string16 text() const {
      if (is_valid_script)
        return UTF8ToUTF16(url.spec() + "!FindProxyForURL");
      if (fetch_error == OK)
        return UTF8ToUTF16(url.spec() + "!invalid-script");
      return string16();
    }

    GURL url;
    int fetch_error;
    bool is_valid_script;
  };

  Rule AddSuccessRule(const char* url) {
    Rule rule(GURL(url), OK /*fetch_error*/, true);
    rules_.push_back(rule);
    return rule;
  }

  void AddFailDownloadRule(const char* url) {
    rules_.push_back(Rule(GURL(url), kFailedDownloading /*fetch_error*/,
        false));
  }

  void AddFailParsingRule(const char* url) {
    rules_.push_back(Rule(GURL(url), OK /*fetch_error*/, false));
  }

  const Rule& GetRuleByUrl(const GURL& url) const {
    for (RuleList::const_iterator it = rules_.begin(); it != rules_.end();
         ++it) {
      if (it->url == url)
        return *it;
    }
    LOG(FATAL) << "Rule not found for " << url;
    return rules_[0];
  }

  const Rule& GetRuleByText(const string16& text) const {
    for (RuleList::const_iterator it = rules_.begin(); it != rules_.end();
         ++it) {
      if (it->text() == text)
        return *it;
    }
    LOG(FATAL) << "Rule not found for " << text;
    return rules_[0];
  }

 private:
  typedef std::vector<Rule> RuleList;
  RuleList rules_;
};

class RuleBasedProxyScriptFetcher : public ProxyScriptFetcher {
 public:
  explicit RuleBasedProxyScriptFetcher(const Rules* rules) : rules_(rules) {}

  // ProxyScriptFetcher implementation.
  virtual int Fetch(const GURL& url,
                    string16* text,
                    const CompletionCallback& callback) {
    const Rules::Rule& rule = rules_->GetRuleByUrl(url);
    int rv = rule.fetch_error;
    EXPECT_NE(ERR_UNEXPECTED, rv);
    if (rv == OK)
      *text = rule.text();
    return rv;
  }

  virtual void Cancel() {}

  virtual URLRequestContext* GetRequestContext() const { return NULL; }

 private:
  const Rules* rules_;
};

// Succeed using custom PAC script.
TEST(ProxyScriptDeciderTest, CustomPacSucceeds) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  Rules::Rule rule = rules.AddSuccessRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  CapturingNetLog log;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, &log);
  EXPECT_EQ(OK, decider.Start(
      config, base::TimeDelta(), true, callback.callback()));
  EXPECT_EQ(rule.text(), decider.script_data()->utf16());

  // Check the NetLog was filled correctly.
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLog::TYPE_PROXY_SCRIPT_DECIDER));

  EXPECT_TRUE(decider.effective_config().has_pac_url());
  EXPECT_EQ(config.pac_url(), decider.effective_config().pac_url());
}

// Fail downloading the custom PAC script.
TEST(ProxyScriptDeciderTest, CustomPacFails1) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  CapturingNetLog log;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, &log);
  EXPECT_EQ(kFailedDownloading,
            decider.Start(config, base::TimeDelta(), true,
                          callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());

  // Check the NetLog was filled correctly.
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLog::TYPE_PROXY_SCRIPT_DECIDER));

  EXPECT_FALSE(decider.effective_config().has_pac_url());
}

// Fail parsing the custom PAC script.
TEST(ProxyScriptDeciderTest, CustomPacFails2) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailParsingRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(kFailedParsing,
            decider.Start(config, base::TimeDelta(), true,
                          callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());
}

// Fail downloading the custom PAC script, because the fetcher was NULL.
TEST(ProxyScriptDeciderTest, HasNullProxyScriptFetcher) {
  Rules rules;
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  TestCompletionCallback callback;
  ProxyScriptDecider decider(NULL, &dhcp_fetcher, NULL);
  EXPECT_EQ(ERR_UNEXPECTED,
            decider.Start(config, base::TimeDelta(), true,
                          callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());
}

// Succeeds in choosing autodetect (WPAD DNS).
TEST(ProxyScriptDeciderTest, AutodetectSuccess) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_auto_detect(true);

  Rules::Rule rule = rules.AddSuccessRule("http://wpad/wpad.dat");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(OK, decider.Start(
      config, base::TimeDelta(), true, callback.callback()));
  EXPECT_EQ(rule.text(), decider.script_data()->utf16());

  EXPECT_TRUE(decider.effective_config().has_pac_url());
  EXPECT_EQ(rule.url, decider.effective_config().pac_url());
}

// Fails at WPAD (downloading), but succeeds in choosing the custom PAC.
TEST(ProxyScriptDeciderTest, AutodetectFailCustomSuccess1) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://wpad/wpad.dat");
  Rules::Rule rule = rules.AddSuccessRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(OK, decider.Start(
      config, base::TimeDelta(), true, callback.callback()));
  EXPECT_EQ(rule.text(), decider.script_data()->utf16());

  EXPECT_TRUE(decider.effective_config().has_pac_url());
  EXPECT_EQ(rule.url, decider.effective_config().pac_url());
}

// Fails at WPAD (no DHCP config, DNS PAC fails parsing), but succeeds in
// choosing the custom PAC.
TEST(ProxyScriptDeciderTest, AutodetectFailCustomSuccess2) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://custom/proxy.pac"));
  config.proxy_rules().ParseFromString("unused-manual-proxy:99");

  rules.AddFailParsingRule("http://wpad/wpad.dat");
  Rules::Rule rule = rules.AddSuccessRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  CapturingNetLog log;

  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, &log);
  EXPECT_EQ(OK, decider.Start(config, base::TimeDelta(),
                          true, callback.callback()));
  EXPECT_EQ(rule.text(), decider.script_data()->utf16());

  // Verify that the effective configuration no longer contains auto detect or
  // any of the manual settings.
  EXPECT_TRUE(decider.effective_config().Equals(
      ProxyConfig::CreateFromCustomPacURL(GURL("http://custom/proxy.pac"))));

  // Check the NetLog was filled correctly.
  // (Note that various states are repeated since both WPAD and custom
  // PAC scripts are tried).
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(10u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
  // This is the DHCP phase, which fails fetching rather than parsing, so
  // there is no pair of SET_PAC_SCRIPT events.
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEvent(
      entries, 3,
      NetLog::TYPE_PROXY_SCRIPT_DECIDER_FALLING_BACK_TO_NEXT_PAC_SOURCE,
      NetLog::PHASE_NONE));
  // This is the DNS phase, which attempts a fetch but fails.
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 4, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 5, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEvent(
      entries, 6,
      NetLog::TYPE_PROXY_SCRIPT_DECIDER_FALLING_BACK_TO_NEXT_PAC_SOURCE,
      NetLog::PHASE_NONE));
  // Finally, the custom PAC URL phase.
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 7, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 8, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 9, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
}

// Fails at WPAD (downloading), and fails at custom PAC (downloading).
TEST(ProxyScriptDeciderTest, AutodetectFailCustomFails1) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://wpad/wpad.dat");
  rules.AddFailDownloadRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(kFailedDownloading,
            decider.Start(config, base::TimeDelta(), true,
                          callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());
}

// Fails at WPAD (downloading), and fails at custom PAC (parsing).
TEST(ProxyScriptDeciderTest, AutodetectFailCustomFails2) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_auto_detect(true);
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://wpad/wpad.dat");
  rules.AddFailParsingRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(kFailedParsing,
            decider.Start(config, base::TimeDelta(), true,
                          callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());
}

// This is a copy-paste of CustomPacFails1, with the exception that we give it
// a 1 millisecond delay. This means it will now complete asynchronously.
// Moreover, we test the NetLog to make sure it logged the pause.
TEST(ProxyScriptDeciderTest, CustomPacFails1_WithPositiveDelay) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  CapturingNetLog log;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, &log);
  EXPECT_EQ(ERR_IO_PENDING,
            decider.Start(config, base::TimeDelta::FromMilliseconds(1),
                      true, callback.callback()));

  EXPECT_EQ(kFailedDownloading, callback.WaitForResult());
  EXPECT_EQ(NULL, decider.script_data());

  // Check the NetLog was filled correctly.
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(6u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_PROXY_SCRIPT_DECIDER_WAIT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_PROXY_SCRIPT_DECIDER_WAIT));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 3, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 4, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 5, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
}

// This is a copy-paste of CustomPacFails1, with the exception that we give it
// a -5 second delay instead of a 0 ms delay. This change should have no effect
// so the rest of the test is unchanged.
TEST(ProxyScriptDeciderTest, CustomPacFails1_WithNegativeDelay) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  DoNothingDhcpProxyScriptFetcher dhcp_fetcher;

  ProxyConfig config;
  config.set_pac_url(GURL("http://custom/proxy.pac"));

  rules.AddFailDownloadRule("http://custom/proxy.pac");

  TestCompletionCallback callback;
  CapturingNetLog log;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, &log);
  EXPECT_EQ(kFailedDownloading,
            decider.Start(config, base::TimeDelta::FromSeconds(-5),
                          true, callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());

  // Check the NetLog was filled correctly.
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_PROXY_SCRIPT_DECIDER_FETCH_PAC_SCRIPT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLog::TYPE_PROXY_SCRIPT_DECIDER));
}

class SynchronousSuccessDhcpFetcher : public DhcpProxyScriptFetcher {
 public:
  explicit SynchronousSuccessDhcpFetcher(const string16& expected_text)
      : gurl_("http://dhcppac/"), expected_text_(expected_text) {
  }

  int Fetch(string16* utf16_text, const CompletionCallback& callback) override {
    *utf16_text = expected_text_;
    return OK;
  }

  void Cancel() override {
  }

  const GURL& GetPacURL() const override {
    return gurl_;
  }

  const string16& expected_text() const {
    return expected_text_;
  }

 private:
  GURL gurl_;
  string16 expected_text_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousSuccessDhcpFetcher);
};

// All of the tests above that use ProxyScriptDecider have tested
// failure to fetch a PAC file via DHCP configuration, so we now test
// success at downloading and parsing, and then success at downloading,
// failure at parsing.

TEST(ProxyScriptDeciderTest, AutodetectDhcpSuccess) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  SynchronousSuccessDhcpFetcher dhcp_fetcher(
      WideToUTF16(L"http://bingo/!FindProxyForURL"));

  ProxyConfig config;
  config.set_auto_detect(true);

  rules.AddSuccessRule("http://bingo/");
  rules.AddFailDownloadRule("http://wpad/wpad.dat");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  EXPECT_EQ(OK, decider.Start(
      config, base::TimeDelta(), true, callback.callback()));
  EXPECT_EQ(dhcp_fetcher.expected_text(),
            decider.script_data()->utf16());

  EXPECT_TRUE(decider.effective_config().has_pac_url());
  EXPECT_EQ(GURL("http://dhcppac/"), decider.effective_config().pac_url());
}

TEST(ProxyScriptDeciderTest, AutodetectDhcpFailParse) {
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);
  SynchronousSuccessDhcpFetcher dhcp_fetcher(
      WideToUTF16(L"http://bingo/!invalid-script"));

  ProxyConfig config;
  config.set_auto_detect(true);

  rules.AddFailParsingRule("http://bingo/");
  rules.AddFailDownloadRule("http://wpad/wpad.dat");

  TestCompletionCallback callback;
  ProxyScriptDecider decider(&fetcher, &dhcp_fetcher, NULL);
  // Since there is fallback to DNS-based WPAD, the final error will be that
  // it failed downloading, not that it failed parsing.
  EXPECT_EQ(kFailedDownloading,
      decider.Start(config, base::TimeDelta(), true, callback.callback()));
  EXPECT_EQ(NULL, decider.script_data());

  EXPECT_FALSE(decider.effective_config().has_pac_url());
}

class AsyncFailDhcpFetcher
    : public DhcpProxyScriptFetcher,
      public base::SupportsWeakPtr<AsyncFailDhcpFetcher> {
 public:
  AsyncFailDhcpFetcher() {}
  ~AsyncFailDhcpFetcher() {}

  int Fetch(string16* utf16_text, const CompletionCallback& callback) override {
    callback_ = callback;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AsyncFailDhcpFetcher::CallbackWithFailure, AsWeakPtr()));
    return ERR_IO_PENDING;
  }

  void Cancel() override {
    callback_.Reset();
  }

  const GURL& GetPacURL() const override {
    return dummy_gurl_;
  }

  void CallbackWithFailure() {
    if (!callback_.is_null())
      callback_.Run(ERR_PAC_NOT_IN_DHCP);
  }

 private:
  GURL dummy_gurl_;
  CompletionCallback callback_;
};

TEST(ProxyScriptDeciderTest, DhcpCancelledByDestructor) {
  // This regression test would crash before
  // http://codereview.chromium.org/7044058/
  // Thus, we don't care much about actual results (hence no EXPECT or ASSERT
  // macros below), just that it doesn't crash.
  Rules rules;
  RuleBasedProxyScriptFetcher fetcher(&rules);

  scoped_ptr<AsyncFailDhcpFetcher> dhcp_fetcher(new AsyncFailDhcpFetcher());

  ProxyConfig config;
  config.set_auto_detect(true);
  rules.AddFailDownloadRule("http://wpad/wpad.dat");

  TestCompletionCallback callback;

  // Scope so ProxyScriptDecider gets destroyed early.
  {
    ProxyScriptDecider decider(&fetcher, dhcp_fetcher.get(), NULL);
    decider.Start(config, base::TimeDelta(), true, callback.callback());
  }

  // Run the message loop to let the DHCP fetch complete and post the results
  // back. Before the fix linked to above, this would try to invoke on
  // the callback object provided by ProxyScriptDecider after it was
  // no longer valid.
  MessageLoop::current()->RunUntilIdle();
}

}  // namespace
}  // namespace net
