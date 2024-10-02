// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_tab_helper.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/content/captive_portal_tab_reloader.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationSimulator;

namespace captive_portal {

namespace {

const char* const kStartUrl = "http://whatever.com/index.html";
const char* const kHttpUrl = "http://whatever.com/";
const char* const kHttpsUrl = "https://whatever.com/";

// Used for cross-process navigations.  Shouldn't actually matter whether this
// is different from kHttpsUrl, but best to keep things consistent.
const char* const kHttpsUrl2 = "https://cross_process.com/";

}  // namespace

class MockCaptivePortalTabReloader : public CaptivePortalTabReloader {
 public:
  MockCaptivePortalTabReloader()
      : CaptivePortalTabReloader(nullptr, nullptr, base::NullCallback()) {}

  MOCK_METHOD1(OnLoadStart, void(bool));
  MOCK_METHOD2(OnLoadCommitted, void(int, net::ResolveErrorInfo));
  MOCK_METHOD0(OnAbort, void());
  MOCK_METHOD1(OnRedirect, void(bool));
  MOCK_METHOD2(OnCaptivePortalResults,
               void(CaptivePortalResult, CaptivePortalResult));
};

class CaptivePortalTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  CaptivePortalTabHelperTest()
      : mock_reloader_(new testing::StrictMock<MockCaptivePortalTabReloader>) {}

  CaptivePortalTabHelperTest(const CaptivePortalTabHelperTest&) = delete;
  CaptivePortalTabHelperTest& operator=(const CaptivePortalTabHelperTest&) =
      delete;

  ~CaptivePortalTabHelperTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    pref_service_.registry()->RegisterBooleanPref(
        embedder_support::kAlternateErrorPagesEnabled, true);
    captive_portal_service_ = std::make_unique<CaptivePortalService>(
        browser_context(), &pref_service_);
    // Load kStartUrl. This ensures that any subsequent navigation to kHttpsUrl2
    // will be properly registered as cross-process. It should be different than
    // the rest of the URLs used, otherwise unit tests will clasify navigations
    // as same document ones, which would be incorrect.
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kStartUrl));

    tab_helper_.reset(new CaptivePortalTabHelper(
        web_contents(), captive_portal_service_.get(), base::DoNothing()));
    tab_helper_->SetTabReloaderForTest(mock_reloader_);
  }

  void TearDown() override {
    tab_helper_.reset();
    captive_portal_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Simulates a successful load of |url|.
  void SimulateSuccess(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();
    EXPECT_CALL(mock_reloader(),
                OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK)))
        .Times(1);
    navigation->Commit();
  }

  // Simulates a connection timeout while requesting |url|.
  void SimulateTimeout(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Fail(net::ERR_TIMED_OUT);
    EXPECT_CALL(
        mock_reloader(),
        OnLoadCommitted(net::ERR_TIMED_OUT, net::ResolveErrorInfo(net::OK)))
        .Times(1);
    navigation->CommitErrorPage();
  }

  // Simulates a secure DNS network error while requesting |url|.
  void SimulateSecureDnsNetworkError(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->SetResolveErrorInfo(net::ResolveErrorInfo(
        net::ERR_CERT_COMMON_NAME_INVALID, true /* is_secure_network_error */));
    navigation->Fail(net::ERR_NAME_NOT_RESOLVED);
    EXPECT_CALL(mock_reloader(),
                OnLoadCommitted(net::ERR_NAME_NOT_RESOLVED,
                                net::ResolveErrorInfo(
                                    net::ERR_CERT_COMMON_NAME_INVALID, true)))
        .Times(1);
    navigation->CommitErrorPage();
  }

  // Simulates an abort while requesting |url|.
  void SimulateAbort(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    navigation->Fail(net::ERR_ABORTED);

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  // Simulates an abort while loading an error page.
  void SimulateAbortTimeout(const GURL& url) {
    EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
        .Times(1);
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();

    EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
    navigation->Fail(net::ERR_TIMED_OUT);
    navigation->AbortCommit();

    // Make sure that above call resulted in abort, for tests that continue
    // after the abort.
    EXPECT_CALL(mock_reloader(), OnAbort()).Times(0);
  }

  CaptivePortalTabHelper* tab_helper() { return tab_helper_.get(); }

  // Simulates a captive portal redirect by calling the Observe method.
  void ObservePortalResult(CaptivePortalResult previous_result,
                           CaptivePortalResult result) {
    CaptivePortalService::Results results;
    results.previous_result = previous_result;
    results.result = result;

    EXPECT_CALL(mock_reloader(),
                OnCaptivePortalResults(previous_result, result))
        .Times(1);
    tab_helper()->Observe(results);
  }

  MockCaptivePortalTabReloader& mock_reloader() { return *mock_reloader_; }

  void SetIsLoginTab() { tab_helper()->SetIsLoginTab(); }

 private:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<CaptivePortalService> captive_portal_service_;
  std::unique_ptr<CaptivePortalTabHelper> tab_helper_;

  // Owned by |tab_helper_|.
  raw_ptr<testing::StrictMock<MockCaptivePortalTabReloader>> mock_reloader_;
};

TEST_F(CaptivePortalTabHelperTest, HttpSuccess) {
  SimulateSuccess(GURL(kHttpUrl));
}

TEST_F(CaptivePortalTabHelperTest, HttpTimeout) {
  SimulateTimeout(GURL(kHttpUrl));
}

TEST_F(CaptivePortalTabHelperTest, HttpsSuccess) {
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsTimeout) {
  SimulateTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsSecureDnsNetworkError) {
  SimulateSecureDnsNetworkError(GURL(kHttpsUrl));
  // Make sure no state was carried over from the secure DNS network error.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, HttpsAbort) {
  SimulateAbort(GURL(kHttpsUrl));
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// A cross-process navigation is aborted by a same-site navigation.
TEST_F(CaptivePortalTabHelperTest, AbortCrossProcess) {
  SimulateAbort(GURL(kHttpsUrl2));
  // Make sure no state was carried over from the abort.
  SimulateSuccess(GURL(kHttpUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Abort while there's a provisional timeout error page loading.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeout) {
  SimulateAbortTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Abort a cross-process navigation while there's a provisional timeout error
// page loading.
TEST_F(CaptivePortalTabHelperTest, AbortTimeoutCrossProcess) {
  SimulateAbortTimeout(GURL(kHttpsUrl2));
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// Opposite case from above - a same-process error page is aborted in favor of
// a cross-process one.
TEST_F(CaptivePortalTabHelperTest, HttpsAbortTimeoutForCrossProcess) {
  SimulateSuccess(GURL(kHttpsUrl));

  SimulateAbortTimeout(GURL(kHttpsUrl));
  // Make sure no state was carried over from the timeout or the abort.
  SimulateSuccess(GURL(kHttpsUrl2));
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

// A provisional same-site navigation is interrupted by a cross-process
// navigation without sending an abort first.
TEST_F(CaptivePortalTabHelperTest, UnexpectedProvisionalLoad) {
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> same_site_navigation =
      NavigationSimulator::CreateRendererInitiated(same_site_url, main_rfh());
  same_site_navigation->Start();
  same_site_navigation->ReadyToCommit();

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation cancels.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> cross_process_navigation =
      NavigationSimulator::CreateBrowserInitiated(cross_process_url,
                                                  web_contents());
  cross_process_navigation->Start();

  // The cross-process navigation fails.
  cross_process_navigation->Fail(net::ERR_FAILED);

  EXPECT_CALL(mock_reloader(),
              OnLoadCommitted(net::ERR_FAILED, net::ResolveErrorInfo(net::OK)))
      .Times(1);
  cross_process_navigation->CommitErrorPage();
}

// Similar to the above test, except the original RenderFrameHost manages to
// commit before its navigation is aborted.
TEST_F(CaptivePortalTabHelperTest, UnexpectedCommit) {
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When ProactivelySwapBrowsingInstance or RenderDocument is enabled on
    // same-site main-frame navigations, the same-site navigation below will
    // create a pending RenderFrameHost, which will be deleted when a cross-site
    // navigation starts (because it also creates a pending RenderFrameHost).
    // This means the case we wanted for this test is not possible with
    // ProactivelySwapBrowsingInstance or RenderDocument, so we should just skip
    // this test.
    return;
  }
  GURL same_site_url = GURL(kHttpUrl);
  GURL cross_process_url = GURL(kHttpsUrl2);

  // A same-site load for the original RenderViewHost starts.
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> same_site_navigation =
      NavigationSimulator::CreateRendererInitiated(same_site_url, main_rfh());
  same_site_navigation->ReadyToCommit();

  // It's unexpectedly interrupted by a cross-process navigation, which starts
  // navigating before the old navigation commits.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(cross_process_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> cross_process_navigation =
      NavigationSimulator::CreateBrowserInitiated(cross_process_url,
                                                  web_contents());
  cross_process_navigation->Start();

  // The cross-process navigation fails.
  cross_process_navigation->Fail(net::ERR_FAILED);

  // The same-site navigation succeeds.
  EXPECT_CALL(mock_reloader(), OnAbort()).Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadStart(same_site_url.SchemeIsCryptographic()))
      .Times(1);
  EXPECT_CALL(mock_reloader(),
              OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK)))
      .Times(1);
  same_site_navigation->Commit();
}

// Simulates navigations for a number of subframes, and makes sure no
// CaptivePortalTabHelper function is called.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframe) {
  GURL url = GURL(kHttpsUrl);

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe1 = rfh_tester->AppendChild("subframe1");

  // Normal load.
  NavigationSimulator::NavigateAndCommitFromDocument(url, subframe1);

  // Timeout.
  content::RenderFrameHost* subframe2 = rfh_tester->AppendChild("subframe2");
  NavigationSimulator::NavigateAndFailFromDocument(url, net::ERR_TIMED_OUT,
                                                   subframe2);

  // Abort.
  content::RenderFrameHost* subframe3 = rfh_tester->AppendChild("subframe3");
  NavigationSimulator::NavigateAndFailFromDocument(url, net::ERR_ABORTED,
                                                   subframe3);
}

// Simulates a subframe erroring out at the same time as a provisional load,
// but with a different error code.  Make sure the TabHelper sees the correct
// error.
TEST_F(CaptivePortalTabHelperTest, HttpsSubframeParallelError) {
  if (content::AreAllSitesIsolatedForTesting()) {
    // http://crbug.com/674734 Fix this test with Site Isolation
    return;
  }
  // URL used by both frames.
  GURL url = GURL(kHttpsUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  // Loads start.
  EXPECT_CALL(mock_reloader(), OnLoadStart(url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> main_frame_navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  std::unique_ptr<NavigationSimulator> subframe_navigation =
      NavigationSimulator::CreateRendererInitiated(url, subframe);
  main_frame_navigation->Start();
  subframe_navigation->Start();

  // Loads return errors.
  main_frame_navigation->Fail(net::ERR_UNEXPECTED);
  subframe_navigation->Fail(net::ERR_TIMED_OUT);

  // Error page load finishes.
  subframe_navigation->CommitErrorPage();
  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_UNEXPECTED,
                                               net::ResolveErrorInfo(net::OK)))
      .Times(1);
  main_frame_navigation->CommitErrorPage();
}

// Simulates an HTTP to HTTPS redirect, which then times out.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpsRedirectTimeout) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(http_url, main_rfh());
  navigation->Start();

  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(true)).Times(1);
  navigation->Redirect(https_url);

  navigation->Fail(net::ERR_TIMED_OUT);

  EXPECT_CALL(mock_reloader(), OnLoadCommitted(net::ERR_TIMED_OUT,
                                               net::ResolveErrorInfo(net::OK)))
      .Times(1);
  navigation->CommitErrorPage();
}

// Simulates an HTTPS to HTTP redirect.
TEST_F(CaptivePortalTabHelperTest, HttpsToHttpRedirect) {
  GURL https_url(kHttpsUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(https_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(https_url, main_rfh());
  navigation->Start();

  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsCryptographic()))
      .Times(1);
  navigation->Redirect(http_url);

  EXPECT_CALL(mock_reloader(),
              OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK)))
      .Times(1);
  navigation->Commit();
}

// Simulates an HTTP to HTTP redirect.
TEST_F(CaptivePortalTabHelperTest, HttpToHttpRedirect) {
  GURL http_url(kHttpUrl);
  EXPECT_CALL(mock_reloader(), OnLoadStart(http_url.SchemeIsCryptographic()))
      .Times(1);
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(http_url, main_rfh());
  navigation->Start();

  EXPECT_CALL(mock_reloader(), OnRedirect(http_url.SchemeIsCryptographic()))
      .Times(1);
  navigation->Redirect(http_url);

  EXPECT_CALL(mock_reloader(),
              OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK)))
      .Times(1);
  navigation->Commit();
}

// Tests that a subframe redirect doesn't reset the timer to kick off a captive
// portal probe for the main frame if the main frame request is taking too long.
TEST_F(CaptivePortalTabHelperTest, SubframeRedirect) {
  GURL http_url(kHttpUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  std::unique_ptr<NavigationSimulator> main_frame_navigation =
      NavigationSimulator::CreateRendererInitiated(http_url, main_rfh());
  std::unique_ptr<NavigationSimulator> subframe_navigation =
      NavigationSimulator::CreateRendererInitiated(http_url, subframe);

  EXPECT_CALL(mock_reloader(), OnLoadStart(false)).Times(1);
  main_frame_navigation->Start();
  subframe_navigation->Start();

  GURL https_url(kHttpsUrl);
  subframe_navigation->Redirect(https_url);

  EXPECT_CALL(mock_reloader(),
              OnLoadCommitted(net::OK, net::ResolveErrorInfo(net::OK)))
      .Times(1);
  main_frame_navigation->Commit();
}

TEST_F(CaptivePortalTabHelperTest, LoginTabLogin) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());
  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabError) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, LoginTabMultipleResultsBeforeLogin) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  SetIsLoginTab();
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_BEHIND_CAPTIVE_PORTAL,
                      RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_TRUE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_NO_RESPONSE, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

TEST_F(CaptivePortalTabHelperTest, NoLoginTab) {
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_INTERNET_CONNECTED, RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_BEHIND_CAPTIVE_PORTAL, RESULT_NO_RESPONSE);
  EXPECT_FALSE(tab_helper()->IsLoginTab());

  ObservePortalResult(RESULT_NO_RESPONSE, RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_helper()->IsLoginTab());
}

}  // namespace captive_portal
