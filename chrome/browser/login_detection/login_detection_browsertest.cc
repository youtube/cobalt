// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/site_isolation/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace login_detection {

class LoginDetectionBrowserTest : public InProcessBrowserTest {
 public:
  LoginDetectionBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kLoginDetection, {}},
         {site_isolation::features::kSiteIsolationForPasswordSites, {}},
         {optimization_guide::features::kOptimizationHints, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_decider->AddHintForTesting(
        GURL("https://www.optguideloggedin.com/page.html"),
        optimization_guide::proto::LOGIN_DETECTION, absl::nullopt);

    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the histograms for the given login detection type to be recorded.
  void ExpectLoginDetectionTypeMetric(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

  // Verifies the histograms for the given login detection type to be recorded
  // with the given expected bucket count for the prerendering test.
  void ExpectLoginDetectionTypeMetric(
      LoginDetectionType type,
      base::HistogramBase::Count expected_bucket_count) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          expected_bucket_count);
  }

 protected:
  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that sites saved manual passworded list are detected correctly.
IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest,
                       NavigateToManualPasswordedSite) {
  GURL test_url(https_test_server_.GetURL("www.saved.com", "/title1.html"));

  // Initial navigation will not be treated as no login.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);

  // Use site isolation to save the site to manual passworded list.
  content::SiteInstance::StartIsolatingSite(
      browser()->profile(), test_url,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED);

  // Subsequent navigation be detected as login.
  ResetHistogramTester();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);

  // Navigations to other subdomains of saved.com are treated as login too.
  ResetHistogramTester();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server_.GetURL("mobile.saved.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);

  ResetHistogramTester();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("saved.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest, PopUpBasedOAuthLoginFlow) {
  // Navigate to the OAuth requestor.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("www.foo.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Create a popup for the navigation flow.
  content::WebContentsAddedObserver web_contents_added_observer;
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::JsReplace(
          "window.open($1, 'oauth_window', 'width=10,height=10');",
          https_test_server_.GetURL("www.oauthprovider.com",
                                    "/title2.html?client_id=123"))));
  auto* popup_contents = web_contents_added_observer.GetWebContents();
  content::TestNavigationObserver observer(popup_contents);
  observer.WaitForNavigationFinished();
  // This popup navigation is treated as not logged-in too.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // When the popup is closed, it will be detected as OAuth login.
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecJs(popup_contents, "window.close()"));
  destroyed_watcher.Wait();
  ExpectLoginDetectionTypeMetric(
      LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
  ResetHistogramTester();

  // Subsequent navigations to the OAuth requestor site will be treated as OAuth
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("www.foo.com", "/title3.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kOauthLogin);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest,
                       OptimizationGuideDetectedBlocklist) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.optguideloggedin.com/page.html")));
  ExpectLoginDetectionTypeMetric(
      LoginDetectionType::kOptimizationGuideDetected);
}

class LoginDetectionPrerenderBrowserTest : public LoginDetectionBrowserTest {
 public:
  LoginDetectionPrerenderBrowserTest() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&LoginDetectionPrerenderBrowserTest::GetWebContents,
                            base::Unretained(this)));
  }
  ~LoginDetectionPrerenderBrowserTest() override = default;
  LoginDetectionPrerenderBrowserTest(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  LoginDetectionPrerenderBrowserTest& operator=(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_->SetUp(embedded_test_server());
    LoginDetectionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return *prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionPrerenderBrowserTest,
                       PrerenderingShouldNotRecordLoginDetectionMetrics) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  ResetHistogramTester();

  const int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  // Login detection metric should not be recorded in prerendering.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 0);

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // Login detection metric should be recorded after activating.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 1);
}

class LoginDetectionFencedFrameBrowserTest : public LoginDetectionBrowserTest {
 public:
  LoginDetectionFencedFrameBrowserTest() = default;
  ~LoginDetectionFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    LoginDetectionBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionFencedFrameBrowserTest,
                       FencedFrameShouldNotRecordLoginDetectionMetrics) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  ResetHistogramTester();

  // Create a fenced frame to ensure that it doesn't record the login detection
  // metrics.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 0);
}

}  // namespace login_detection
