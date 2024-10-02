// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

using ReductionPolicyState =
    embedder_support::UserAgentReductionEnterprisePolicyState;
using ForceMajorVersionToMinorPolicyState =
    embedder_support::ForceMajorVersionToMinorPosition;

class UserAgentBrowserTest : public InProcessBrowserTest,
                             public testing::WithParamInterface<bool> {
 public:
  UserAgentBrowserTest() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &UserAgentBrowserTest::MonitorUserAgent, base::Unretained(this)));
  }

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kReduceUserAgent);
    }

    ASSERT_TRUE(embedded_test_server()->Start());
    empty_url_ = embedded_test_server()->GetURL("/empty.html");

    InProcessBrowserTest::SetUp();
  }

  void set_user_agent_reduction_policy(ReductionPolicyState policy) {
    browser()->profile()->GetPrefs()->SetInteger(prefs::kUserAgentReduction,
                                                 static_cast<int>(policy));
  }

  ReductionPolicyState user_agent_reduction_policy() {
    return static_cast<ReductionPolicyState>(
        browser()->profile()->GetPrefs()->GetInteger(
            prefs::kUserAgentReduction));
  }

  void set_force_major_version_to_minor_policy(
      ForceMajorVersionToMinorPolicyState policy) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kForceMajorVersionToMinorPositionInUserAgent,
        static_cast<int>(policy));
  }

  std::string observed_user_agent() { return observered_user_agent_; }

  GURL empty_url() { return empty_url_; }

 private:
  void MonitorUserAgent(const net::test_server::HttpRequest& request) {
    if (request.GetURL() == empty_url()) {
      observered_user_agent_ =
          request.headers.find(net::HttpRequestHeaders::kUserAgent)->second;
    }
  }

  std::string observered_user_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL empty_url_;
};

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, EnterprisePolicyInitialized) {
  // Check that default is set correctly
  EXPECT_EQ(ReductionPolicyState::kDefault, user_agent_reduction_policy());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ReductionPolicyDisabled) {
  set_user_agent_reduction_policy(ReductionPolicyState::kForceDisabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(),
            embedder_support::GetFullUserAgent(
                embedder_support::ForceMajorVersionToMinorPosition::kDefault));
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ReductionPolicyEnabled) {
  set_user_agent_reduction_policy(ReductionPolicyState::kForceEnabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(), embedder_support::GetReducedUserAgent());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ReductionPolicyDefault) {
  set_user_agent_reduction_policy(ReductionPolicyState::kDefault);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(), embedder_support::GetUserAgent());
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ForceMajorToMinorPolicyDisabled) {
  set_force_major_version_to_minor_policy(
      ForceMajorVersionToMinorPolicyState::kForceDisabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(),
            embedder_support::GetUserAgent(
                ForceMajorVersionToMinorPolicyState::kForceDisabled));
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ForceMajorToMinorPolicyEnabled) {
  set_force_major_version_to_minor_policy(
      ForceMajorVersionToMinorPolicyState::kForceEnabled);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(),
            embedder_support::GetUserAgent(
                ForceMajorVersionToMinorPolicyState::kForceEnabled));
}

IN_PROC_BROWSER_TEST_P(UserAgentBrowserTest, ForceMajorToMinorPolicyDefault) {
  set_force_major_version_to_minor_policy(
      ForceMajorVersionToMinorPolicyState::kDefault);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), empty_url()));
  EXPECT_EQ(observed_user_agent(),
            embedder_support::GetUserAgent(
                ForceMajorVersionToMinorPolicyState::kDefault));
}

INSTANTIATE_TEST_SUITE_P(ReduceUserAgentFeature,
                         UserAgentBrowserTest,
                         ::testing::Bool());

class ReduceUserAgentPlatformBrowserTest : public InProcessBrowserTest {
 public:
  ReduceUserAgentPlatformBrowserTest() = default;

  void SetUp() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "ReduceUserAgentMinorVersion,ReduceUserAgentPlatformOsCpu", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReduceUserAgentPlatformBrowserTest, NavigatorPlatform) {
  // We should not reduce android navigator.platform in phase 5.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE("Linux x86_64",
            content::EvalJs(web_contents(), "navigator.platform"));
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ("MacIntel", content::EvalJs(web_contents(), "navigator.platform"));
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ("Win32", content::EvalJs(web_contents(), "navigator.platform"));
#else
  EXPECT_EQ("Linux x86_64",
            content::EvalJs(web_contents(), "navigator.platform"));
#endif
}

}  // namespace policy
