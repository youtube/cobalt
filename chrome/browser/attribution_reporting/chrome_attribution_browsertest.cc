// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

// Tests for the Conversion Measurement API that rely on chrome/ layer features.
// UseCounter recording and multiple browser window behavior is not available
// content shell.
class ChromeAttributionBrowserTest : public InProcessBrowserTest {
 public:
  ChromeAttributionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPrivacySandboxAdsAPIsOverride);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for ConversionMeasurement.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Debug mode is needed to skip delays and noise which allows to assert
    // that reports are received when expected.
    command_line->AppendSwitch(switches::kAttributionReportingDebugMode);
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&server_);
    server_.ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(&server_);
  }

 protected:
  static constexpr char kReportEndpoint[] =
      "https://c.test/.well-known/attribution-reporting/"
      "report-event-attribution";

  content::WebContents* RegisterSourceWithNavigation() {
    content::WebContentsAddedObserver window_observer;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        server_.GetURL(
            "a.test",
            "/attribution_reporting/page_with_impression_creator.html")));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    GURL register_url = server_.GetURL(
        "c.test", "/attribution_reporting/register_source_headers.html");
    GURL link_url = server_.GetURL(
        "d.test", "/attribution_reporting/page_with_conversion_redirect.html");
    // Navigate the page using window.open and set an attribution source.
    EXPECT_TRUE(
        ExecJs(web_contents, content::JsReplace(R"(
    window.open($1, "_blank", "attributionsrc="+$2);)",
                                                link_url, register_url)));

    content::WebContents* new_contents = window_observer.GetWebContents();
    WaitForLoadStop(new_contents);
    return new_contents;
  }

  void RegisterTrigger(content::WebContents* contents) {
    GURL register_trigger_url = server_.GetURL(
        "c.test", "/attribution_reporting/register_trigger_headers.html");
    EXPECT_TRUE(
        ExecJs(contents, content::JsReplace("createAttributionSrcImg($1);",
                                            register_trigger_url)));
  }

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;
};

struct ExpectedReportWaiter {
  // ControllableHTTPResponses can only wait for relative urls, so only supply
  // the path.
  ExpectedReportWaiter(GURL report_url, net::EmbeddedTestServer* server)
      : expected_url(std::move(report_url)),
        response(std::make_unique<net::test_server::ControllableHttpResponse>(
            server,
            expected_url.path())) {}

  GURL expected_url;
  std::unique_ptr<net::test_server::ControllableHttpResponse> response;

  // Waits for a report to be received matching the report url.
  void WaitForRequest() {
    if (!HasRequest()) {
      response->WaitForRequest();
    }
    EXPECT_TRUE(response->http_request()->has_content);
  }

  bool HasRequest() { return response->http_request(); }
};

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       WindowOpenWithOnlyAttributionFeatures_LinkOpenedInTab) {
  ASSERT_TRUE(server_.Start());

  base::HistogramTester histogram_tester;

  content::WebContents* new_contents = RegisterSourceWithNavigation();

  // Ensure the window was opened in a new tab. If the window is in a new popup
  // the web contents would not belong to the tab strip.
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetIndexOfWebContents(new_contents));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
}

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       AttestedSiteCanReceiveAttributionReport) {
  PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
      ->SetAllPrivacySandboxAllowedForTesting();

  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(net::SchemefulSite(GURL("https://c.test")),
                       privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
                           privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                               kAttributionReporting});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  ExpectedReportWaiter expected_report(GURL(kReportEndpoint), &server_);
  ASSERT_TRUE(server_.Start());

  auto* new_contents = RegisterSourceWithNavigation();

  content::WebContentsConsoleObserver console_observer(new_contents);
  console_observer.SetPattern(
      "Attestation check for Attribution Reporting on * failed.");

  RegisterTrigger(new_contents);

  expected_report.WaitForRequest();
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(ChromeAttributionBrowserTest,
                       UnattestedSiteCannotReceiveAttributionReport) {
  PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
      ->SetAllPrivacySandboxAllowedForTesting();

  // We add an empty attestation for the reporting origin. If feature
  // `kDefaultAllowPrivacySandboxAttestations` is enabled, the attestation is
  // default allowed if the map is absent. Creating a map with the reporting
  // endpoint mapping to an empty set will make sure the attestation will fail.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(
          privacy_sandbox::PrivacySandboxAttestationsMap{
              {net::SchemefulSite(GURL(kReportEndpoint)), {}}});

  ExpectedReportWaiter expected_report(GURL(kReportEndpoint), &server_);
  ASSERT_TRUE(server_.Start());

  auto* new_contents = RegisterSourceWithNavigation();

  content::WebContentsConsoleObserver console_observer(new_contents);
  console_observer.SetPattern(
      "Attestation check for Attribution Reporting on * failed.");

  RegisterTrigger(new_contents);

  // Wait to 2 seconds as reports should be received by then and we want to
  // limit the test duration.
  base::RunLoop loop_;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Seconds(2), loop_.QuitClosure());
  loop_.Run();
  EXPECT_FALSE(expected_report.HasRequest());

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}
