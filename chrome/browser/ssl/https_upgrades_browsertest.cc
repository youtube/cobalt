// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/browser/ssl/https_upgrades_interceptor.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

using security_interstitials::https_only_mode::Event;
using security_interstitials::https_only_mode::InterstitialReason;
using security_interstitials::https_only_mode::kEventHistogram;
using security_interstitials::https_only_mode::
    kEventHistogramWithEngagementHeuristic;
using security_interstitials::https_only_mode::kInterstitialReasonHistogram;
using security_interstitials::https_only_mode::
    kNavigationRequestSecurityLevelHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicAccumulatedHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicEnforcementDurationHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicHostCountHistogram;
using security_interstitials::https_only_mode::
    kSiteEngagementHeuristicStateHistogram;
using security_interstitials::https_only_mode::NavigationRequestSecurityLevel;
using security_interstitials::https_only_mode::SiteEngagementHeuristicState;

// Many of the following tests have only minor variations for HTTPS-First Mode
// vs. HTTPS-Upgrades. These get parameterized so the tests run under both
// versions on their own as well as when both HTTPS Upgrades and HTTPS-First
// Mode are enabled (to test any interactions between the two upgrade modes).
// The code also needs to be able to run safely when neither HTTPS-First Mode
// nor HTTPS-Upgrades are enabled.
//
// Quick summary of all features tested here:
// * HTTPS-Upgrades:
//     Automatically upgrades main frame navigations to HTTPS. Silently falls
//     back to HTTP on failure.
// * HTTPS First Mode:
//     Automatically upgrades main frame navigations to HTTPS. Shows an
//     interstitial on failure.
// * HTTPS First Mode With Site Engagement:
//     Automatically enables HTTPS First Mode for sites that are visited mainly
//     over HTTPS. Requires HTTPS-Upgrades to be enabled.
// * HTTPS First Mode for Typically Secure Users
//     Automatically enables HTTPS First Mode for users that mainly visit HTTPS
//     sites. Requires HTTPS-Upgrades to be enabled.
//
enum class HttpsUpgradesTestType {
  // Enables HFM pref.
  kHttpsFirstModeOnly,
  // Enables HTTPS Upgrades feature flag.
  kHttpsUpgradesOnly,
  // Enables both the HFM pref and HTTPS Upgrades feature flag.
  kHttpsFirstModeAndHttpsUpgrades,
  // Enables HFM with Site Engagement heuristic and HTTPS Upgrades feature flag.
  // HTTPS Upgrades is a prerequisite for HFM with Site Engagement heuristic.
  kHttpsFirstModeWithSiteEngagementAndHttpsUpgrades,

  // Enables HFM for Typically Secure Users and HTTPS Upgrades feature flag.
  // HTTPS Upgrades is a prerequisite for HFM for Typically Secure users.
  kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades,

  // Enables HFM with Site Engagement and HFM for Typically Secure Users (both
  // automatically enable HFM) and HTTPS upgrades feature flag.
  kAllAutoHFM,

  // Enables HFM pref, HFM with Site Engagement heuristic, HFM for typically
  // secure users and HTTPS upgrades feature flag.
  kAll,
  // Disables HFM pref, HFM with Site Engagement heuristic, HFM for typically
  // secure users and HTTPS Upgrades feature.
  kNeither,
};

// Stores the number of times the HTTPS-First Mode interstitial is shown for the
// given reason.
struct ExpectedInterstitialReasons {
  // The number of times the interstitial was shown because the HFM pref was
  // enabled.
  size_t pref = 0;
  // The number of times the interstitial was shown because of the Typically
  // Secure User heuristic.
  size_t typically_secure_user = 0;
};

// A very low site engagement score.
constexpr int kLowSiteEngagementScore = 2;
// A very high site engagement score.
constexpr int kHighSiteEnagementScore = 95;

// Tests for HTTPS-Upgrades and the v2 implementation of HTTPS-First Mode.
class HttpsUpgradesBrowserTest
    : public testing::WithParamInterface<HttpsUpgradesTestType>,
      public InProcessBrowserTest {
 public:
  HttpsUpgradesBrowserTest() = default;
  ~HttpsUpgradesBrowserTest() override = default;

  void SetUp() override {
    // HFM is controlled by a pref (configured in SetUpOnMainThread).
    switch (https_upgrades_test_type()) {
      case HttpsUpgradesTestType::kHttpsFirstModeOnly:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                features::kHttpsUpgrades,
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case HttpsUpgradesTestType::kHttpsUpgradesOnly:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case HttpsUpgradesTestType::kHttpsFirstModeAndHttpsUpgrades:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case HttpsUpgradesTestType::
          kHttpsFirstModeWithSiteEngagementAndHttpsUpgrades:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades,
                                  features::kHttpsFirstModeV2ForEngagedSites},
            /*disabled_features=*/{
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;

      case HttpsUpgradesTestType::
          kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades,
                                  features::
                                      kHttpsFirstModeV2ForTypicallySecureUsers},
            /*disabled_features=*/{features::kHttpsFirstModeV2ForEngagedSites});
        break;

      case HttpsUpgradesTestType::kAllAutoHFM:
        // HFM pref is disabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades,
                                  features::
                                      kHttpsFirstModeV2ForTypicallySecureUsers,
                                  features::kHttpsFirstModeV2ForEngagedSites},
            /*disabled_features=*/{});
        break;

      // Enable HTTPS-Upgrades, HFM and HFM with Site Engagement heuristic.
      case HttpsUpgradesTestType::kAll:
        // HFM pref is enabled in SetUpOnMainThread.
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{features::kHttpsUpgrades,
                                  features::kHttpsFirstModeV2ForEngagedSites,
                                  features::
                                      kHttpsFirstModeV2ForTypicallySecureUsers},
            /*disabled_features=*/{});
        break;

      // Disable HTTPS-Upgrades, HFM and HFM with Site Engagement heuristic.
      // (HFM pref is disabled in SetUpOnMainThread.)
      case HttpsUpgradesTestType::kNeither:
        feature_list_.InitWithFeatures(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                features::kHttpsUpgrades,
                features::kHttpsFirstModeV2ForEngagedSites,
                features::kHttpsFirstModeV2ForTypicallySecureUsers});
        break;
    }

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // By default allow all hosts on HTTPS.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up "bad-https.com" as a hostname with an SSL error. HTTPS upgrades
    // to this host will fail.
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = false;
    verify_result.verified_cert = cert;
    verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "bad-https.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);
    mock_cert_verifier_.mock_cert_verifier()->AddResultForCertAndHost(
        cert, "www.bad-https.com", verify_result,
        net::ERR_CERT_COMMON_NAME_INVALID);

    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(https_server_.Start());

    HttpsUpgradesInterceptor::SetHttpsPortForTesting(https_server()->port());
    HttpsUpgradesInterceptor::SetHttpPortForTesting(http_server()->port());

    // Only enable the HTTPS-First Mode pref when the test config calls for it.
    // Some of the HFM heuristics check that the preference wasn't set so as
    // not to override user preference (e.g. if the user changed the pref by
    // turning it off from the UI, we don't want to override it).
    if (IsHttpsFirstModePrefEnabled()) {
      SetPref(IsHttpsFirstModePrefEnabled());
    }
  }

  void TearDownOnMainThread() override { SetPref(false); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  HttpsUpgradesTestType https_upgrades_test_type() const { return GetParam(); }

  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  }

  void ProceedThroughInterstitial(content::WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecJs(tab, javascript));
    nav_observer.Wait();
  }

  void DontProceedThroughInterstitial(content::WebContents* tab) {
    content::TestNavigationObserver nav_observer(tab, 1);
    std::string javascript =
        "window.certificateErrorPageController.dontProceed();";
    ASSERT_TRUE(content::ExecJs(tab, javascript));
    nav_observer.Wait();
  }

  void NavigateAndWaitForFallback(content::WebContents* tab, const GURL& url) {
    // TODO(crbug.com/1394910): With fallback as part of the same navigation,
    // this helper is no longer particularly useful. Consider updating callers.
    content::NavigateToURLBlockUntilNavigationsComplete(tab, url, 1);
  }

  // Whether HFM is enabled by the UI setting and the tests should run steps
  // that assume the HTTP interstitial will trigger (i.e., for fallback HTTP
  // navigations when HTTPS-First Mode is enabled).
  bool IsHttpsFirstModePrefEnabled() const {
    return https_upgrades_test_type() ==
               HttpsUpgradesTestType::kHttpsFirstModeOnly ||
           https_upgrades_test_type() ==
               HttpsUpgradesTestType::kHttpsFirstModeAndHttpsUpgrades ||
           https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
  }

  // Whether HTTPS-First Mode with Site Engagement Heuristic is enabled. When
  // enabled, this feature will enable HFM on sites that have high Site
  // Engagement scores on their HTTPS URLs. HFM with Site Engagement requires
  // HTTPS-Upgrades to be enabled.
  bool IsSiteEngagementHeuristicEnabled() const {
    bool enabled =
        https_upgrades_test_type() ==
            HttpsUpgradesTestType::
                kHttpsFirstModeWithSiteEngagementAndHttpsUpgrades ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
    if (enabled) {
      DCHECK(IsHttpUpgradingEnabled());
    }
    return enabled;
  }

  // Whether automatic HTTPS-First Mode for typically secure users is enabled.
  // When enabled, this feature will enable HFM for users who would see HFM
  // warnings very rarely. HFM for typically secure users requires
  // HTTPS-Upgrades to be enabled.
  bool IsTypicallySecureUserFeatureEnabled() const {
    bool enabled =
        https_upgrades_test_type() ==
            HttpsUpgradesTestType::
                kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM ||
        https_upgrades_test_type() == HttpsUpgradesTestType::kAll;
    if (enabled) {
      DCHECK(IsHttpUpgradingEnabled());
    }
    return enabled;
  }

  // Whether the tests should run steps that assume HTTP upgrading will trigger.
  bool IsHttpUpgradingEnabled() const {
    return https_upgrades_test_type() != HttpsUpgradesTestType::kNeither;
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(browser()->profile());
    service->ResetBaseScoreForURL(url, score);
    ASSERT_EQ(score, service->GetScore(url));
  }

  // Checks that the HTTPS-First Mode interstitial has been shown for the
  // correct reasons.
  void CheckInterstitialReasonHistogram(
      const ExpectedInterstitialReasons& expected_reasons) {
    histograms()->ExpectTotalCount(
        kInterstitialReasonHistogram,
        expected_reasons.pref + expected_reasons.typically_secure_user);
    histograms()->ExpectBucketCount(kInterstitialReasonHistogram,
                                    static_cast<int>(InterstitialReason::kPref),
                                    expected_reasons.pref);
    histograms()->ExpectBucketCount(
        kInterstitialReasonHistogram,
        static_cast<int>(InterstitialReason::kTypicallySecureUserHeuristic),
        expected_reasons.typically_secure_user);
  }

  net::EmbeddedTestServer* http_server() { return &http_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }
  base::HistogramTester* histograms() { return &histograms_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer http_server_{net::EmbeddedTestServer::TYPE_HTTP};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::ContentMockCertVerifier mock_cert_verifier_;
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HttpsUpgradesBrowserTest,
    ::testing::Values(
        HttpsUpgradesTestType::kHttpsFirstModeOnly,
        HttpsUpgradesTestType::kHttpsUpgradesOnly,
        HttpsUpgradesTestType::kHttpsFirstModeAndHttpsUpgrades,
        HttpsUpgradesTestType::
            kHttpsFirstModeWithSiteEngagementAndHttpsUpgrades,
        HttpsUpgradesTestType::
            kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades,
        HttpsUpgradesTestType::kAllAutoHFM,
        HttpsUpgradesTestType::kAll,
        HttpsUpgradesTestType::kNeither),
    // Map param to a human-readable string for better test output.
    [](testing::TestParamInfo<HttpsUpgradesTestType> input_type)
        -> std::string {
      switch (input_type.param) {
        case HttpsUpgradesTestType::kHttpsFirstModeOnly:
          return "HttpsFirstModeOnly";
        case HttpsUpgradesTestType::kHttpsUpgradesOnly:
          return "HttpsUpgradesOnly";
        case HttpsUpgradesTestType::kHttpsFirstModeAndHttpsUpgrades:
          return "HttpsFirstModeAndHttpsUpgrades";
        case HttpsUpgradesTestType::
            kHttpsFirstModeWithSiteEngagementAndHttpsUpgrades:
          return "HttpsFirstModeWithSiteEngagementAndHttpsUpgrades";
        case HttpsUpgradesTestType::
            kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades:
          return "HttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades";
        case HttpsUpgradesTestType::kAllAutoHFM:
          return "AllAutoHFM";
        case HttpsUpgradesTestType::kAll:
          return "AllFeatures";
        case HttpsUpgradesTestType::kNeither:
          return "NeitherFeature";
      }
    });

// If the user navigates to an HTTP URL for a site that supports HTTPS, the
// navigation should end up on the HTTPS version of the URL if upgrading is
// enabled.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpScheme_ShouldUpgrade) {
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");

  // The NavigateToURL() call returns `false` because the navigation is
  // redirected to HTTPS.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver nav_observer(contents, 1);
  if (IsHttpUpgradingEnabled()) {
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  } else {
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  }
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

  if (IsHttpUpgradingEnabled()) {
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectTotalCount(kEventHistogram, 2);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted,
                                    1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded,
                                    1);

    // Also record general request metrics.
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 1);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);

    // Also record general request metrics.
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kInsecure,
                                    1);
  }
}

// If the user navigates to an HTTPS URL for a site that supports HTTPS, the
// navigation should end up on that exact URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpsScheme_ShouldLoad) {
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, https_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // General navigation metrics should still be recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// If the user navigates to a localhost URL, the navigation should end up on
// that exact URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, Localhost_ShouldNotUpgrade) {
  GURL localhost_url = http_server()->GetURL("localhost", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, localhost_url));

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // Verify that general navigation request metrics were recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kLocalhost,
                                  1);
}

// Test that HTTPS Upgrades are skipped for non-unique hostnames, such as
// non-publicly routable (RFC1918/4193) IP addresses, but HTTPS-First Mode
// should still apply.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NonRoutableIPAddress_ShouldNotUpgrade) {
  // This test is only interesting for HTTPS-Upgrades and HTTPS-First Mode.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  // Set up an interceptor because the test server can't listen on private IPs.
  GURL local_ip_url("http://192.168.0.1/simple.html");
  auto url_loader_interceptor =
      content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
          GetChromeTestDataDir().MaybeAsASCII(),
          local_ip_url.GetWithEmptyPath());

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  if (IsHttpsFirstModePrefEnabled()) {
    // HFM should attempt the upgrade, fail, and fallback to the interstitial.
    EXPECT_FALSE(content::NavigateToURL(contents, local_ip_url));
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Verify that upgrade events were recorded because an upgrade was attempted
    // and failed.
    histograms()->ExpectTotalCount(kEventHistogram, 3);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeAttempted, 1);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(
        kEventHistogram,
        security_interstitials::https_only_mode::Event::kUpgradeTimedOut, 1);
  } else {
    // If HFM is not enabled, HTTPS-Upgrades should not attempt to upgrade the
    // navigation.
    EXPECT_TRUE(content::NavigateToURL(contents, local_ip_url));
    histograms()->ExpectTotalCount(kEventHistogram, 0);
  }

  histograms()->ExpectBucketCount(
      kNavigationRequestSecurityLevelHistogram,
      NavigationRequestSecurityLevel::kNonUniqueHostname, 1);
}

// If the user navigates to a non-unique hostname, the navigation should be
// upgraded, but record insecure metrics.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, NonUniqueHost_RecordsMetrics) {
  GURL nonunique_url1 = http_server()->GetURL("test.local", "/simple.html");
  GURL nonunique_url2 = http_server()->GetURL("test", "/simple.html");
  // Note that we don't test with an RFC1918 IP because the test server
  // wouldn't receive the traffic (since it relies on DNS).

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_FALSE(content::NavigateToURL(contents, nonunique_url1));
    EXPECT_FALSE(content::NavigateToURL(contents, nonunique_url2));
    // Other histograms are still recorded.
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kSecure, 2);
  } else if (IsHttpUpgradingEnabled()) {
    // When HFM is not enabled but upgrading is, Chrome does NOT upgrade, so
    // other histograms are not recorded.
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url1));
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url2));
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  } else {
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url1));
    EXPECT_TRUE(content::NavigateToURL(contents, nonunique_url2));
    // Other histograms are still recorded.
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kInsecure,
                                    2);
  }

  histograms()->ExpectBucketCount(
      kNavigationRequestSecurityLevelHistogram,
      NavigationRequestSecurityLevel::kNonUniqueHostname, 2);
}

// If the user navigates to an HTTPS URL, the navigation should end up on that
// exact URL, even if the site has an SSL error.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpsScheme_BrokenSSL_ShouldNotFallback) {
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(content::NavigateToURL(contents, https_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingSSLInterstitial(contents));
  }

  // Verify that navigation event metrics were not recorded as the navigation
  // was not upgraded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// If the user navigates to an HTTP URL for a site with broken HTTPS, the
// navigation should end up on the HTTPS URL and show the HTTPS-Only Mode
// interstitial.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       UrlWithHttpScheme_BrokenSSL_ShouldInterstitial) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted,
                                    1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError,
                                    1);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }
}

// TODO(https://crbug.com/1435222): Fails on the linux-wayland-rel bot.
#if defined(OZONE_PLATFORM_WAYLAND)
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_SiteEngagement \
  DISABLED_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_SiteEngagement
#else
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_SiteEngagement \
  UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_SiteEngagement
#endif
// Test for Site Engagement Heuristic, a feature that enables HFM on specific
// sites based on their site engagement scores.
// If the user navigates to an HTTP URL for a site with broken HTTPS, the
// navigation should end up on the HTTPS URL and show the HTTPS-Only Mode
// interstitial. It should also record a separate histogram for Site Engagement
// Heuristic if the interstitial isn't enabled.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_SiteEngagement) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set test clock.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);

  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled() || IsSiteEngagementHeuristicEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
        contents->GetPrimaryMainFrame(),
        IsSiteEngagementHeuristicEnabled()
            ? "You usually connect to this site securely"
            : "You are seeing this warning because this site does not support "
              "HTTPS."));
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted,
                                    1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError,
                                    1);

    // Check engagement heuristic metrics. These are only recorded when the
    // interstitial isn't enabled by the user pref.
    if (!IsHttpsFirstModePrefEnabled()) {
      histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 3);
      histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                      Event::kUpgradeAttempted, 1);
      histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                      Event::kUpgradeFailed, 1);
      histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                      Event::kUpgradeCertError, 1);
    } else {
      histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
    }

  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);

    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 1);
    histograms()->ExpectBucketCount(kEventHistogramWithEngagementHeuristic,
                                    Event::kUpgradeNotAttempted, 1);
  }

  histograms()->ExpectTotalCount(kSiteEngagementHeuristicStateHistogram, 1);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                  SiteEngagementHeuristicState::kDisabled, 0);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicStateHistogram,
                                  SiteEngagementHeuristicState::kEnabled, 1);

  // Check host count.
  histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 1);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram, 0,
                                  /*count=*/0);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram, 1,
                                  /*count=*/1);
  // Check accumulated host count.
  histograms()->ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1);
  histograms()->ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0, /*count=*/0);
  histograms()->ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1, /*count=*/1);
  // Check enforcement duration. Since the host isn't removed from HFM
  // enforcement list, no duration should be recorded yet.
  histograms()->ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 0);

  // Lower HTTPS engagement score. This disables HFM on the site. Also advance
  // the clock.
  SetSiteEngagementScore(https_url, 5);
  clock_ptr->Advance(base::Hours(1));
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // Should only show the interstitial if the HFM pref is enabled. Site
  // engagement heuristic alone will no longer cause an interstitial.
  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceed through the interstitial, which will add the host to the
    // allowlist and navigate to the HTTP fallback URL.
    ProceedThroughInterstitial(contents);

    // Verify that the interstitial metrics were correctly recorded. The
    // interstitial was shown twice, once clicked through and once not.
    histograms()->ExpectTotalCount("interstitial.https_first_mode.decision", 4);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 2);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    if (IsSiteEngagementHeuristicEnabled()) {
      // Verify that the interstitial metrics were correctly recorded. The
      // interstitial was once and navigated away from.
      histograms()->ExpectTotalCount("interstitial.https_first_mode.decision",
                                     2);
      histograms()->ExpectBucketCount(
          "interstitial.https_first_mode.decision",
          security_interstitials::MetricsHelper::Decision::SHOW, 1);
      histograms()->ExpectBucketCount(
          "interstitial.https_first_mode.decision",
          security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
    } else {
      histograms()->ExpectTotalCount("interstitial.https_first_mode.decision",
                                     0);
    }
  }

  // Event histogram shouldn't change because Site Engagement heuristic didn't
  // kick in.
  if (IsHttpUpgradingEnabled()) {
    if (!IsHttpsFirstModePrefEnabled()) {
      histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 3);
    } else {
      histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
    }
  } else {
    histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 1);
  }

  // Check host count.
  histograms()->ExpectTotalCount(kSiteEngagementHeuristicHostCountHistogram, 2);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram, 0,
                                  /*count=*/1);
  histograms()->ExpectBucketCount(kSiteEngagementHeuristicHostCountHistogram, 1,
                                  /*count=*/1);
  // Check accumulated host count.
  histograms()->ExpectTotalCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 2);
  histograms()->ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 0, /*count=*/0);
  histograms()->ExpectBucketCount(
      kSiteEngagementHeuristicAccumulatedHostCountHistogram, 1, /*count=*/2);
  // Check enforcement duration. The host is now removed from HFM
  // enforcement list, so its HFM enforcement duration should be recorded now.
  histograms()->ExpectTotalCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, 1);
  histograms()->ExpectTimeBucketCount(
      kSiteEngagementHeuristicEnforcementDurationHistogram, base::Hours(1), 1);
}

// TODO(https://crbug.com/1469343): Fails on the linux-wayland-rel bot.
#if defined(OZONE_PLATFORM_WAYLAND)
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser \
  DISABLED_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser
#else
#define MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser \
  UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser
#endif
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    MAYBE_UrlWithHttpScheme_BrokenSSL_ShouldInterstitial_TypicallySecureUser) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // Typically Secure User heuristic requires a minimum total site engagement
  // score.
  SetSiteEngagementScore(GURL("https://google.com"), 90);

  // Set test clock.
  base::SimpleTestClock clock;

  HttpsFirstModeService* hfm_service =
      HttpsFirstModeServiceFactory::GetForProfile(profile);
  hfm_service->SetClockForTesting(&clock);
  base::Time now = base::Time::NowFromSystemTime();

  // Start the clock at standard system time.
  clock.SetNow(now);
  profile->SetCreationTimeForTesting(now);

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  // Visit the HTTP URL. Profile age isn't old enough so Typically Secure
  // Users feature won't show an interstitial here.
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  ExpectedInterstitialReasons expected_reasons;

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
        contents->GetPrimaryMainFrame(),
        "You are seeing this warning because this site does not support "
        "HTTPS."));

    expected_reasons.pref++;
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Move the clock forward and revisit HTTP. Profile is old enough now, so
  // Typically Secure Users feature will auto-enable HFM and show an
  // interstitial.
  clock.SetNow(base::Time::NowFromSystemTime() + base::Days(15));
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  bool expect_interstitial =
      IsHttpsFirstModePrefEnabled() || IsTypicallySecureUserFeatureEnabled();
  // Expect typically secure text only when HFM is auto-enabled, so exclude
  // HttpsUpgradesTestType::kAll where HFM is enabled via pref).
  bool expect_typically_secure_user_interstitial_text =
      https_upgrades_test_type() ==
          HttpsUpgradesTestType::
              kHttpsFirstModeForTypicallySecureUsersAndHttpsUpgrades ||
      https_upgrades_test_type() == HttpsUpgradesTestType::kAllAutoHFM;

  if (expect_interstitial) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
        contents->GetPrimaryMainFrame(),
        expect_typically_secure_user_interstitial_text
            ? "You usually connect to sites securely"
            : "You are seeing this warning because this site does not support "
              "HTTPS."));

    if (expect_typically_secure_user_interstitial_text) {
      expected_reasons.typically_secure_user++;
    } else {
      expected_reasons.pref++;
    }
  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Move the clock forward a day and revisit HTTP. Should still show HFM
  // interstitial.
  clock.SetNow(base::Time::NowFromSystemTime() + base::Days(16));
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (expect_interstitial) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
        contents->GetPrimaryMainFrame(),
        expect_typically_secure_user_interstitial_text
            ? "You usually connect to sites securely"
            : "You are seeing this warning because this site does not support "
              "HTTPS."));

    if (expect_typically_secure_user_interstitial_text) {
      expected_reasons.typically_secure_user++;
    } else {
      expected_reasons.pref++;
    }

  } else {
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  CheckInterstitialReasonHistogram(expected_reasons);

  // Disable HFM. Should no longer auto-enable it.
  SetPref(false);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Re-enable HFM. Should now show HFM interstitial without the auto-enabled
  // text.
  SetPref(true);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      contents->GetPrimaryMainFrame(),
      "You are seeing this warning because this site does not support "
      "HTTPS."));
  expected_reasons.pref++;

  CheckInterstitialReasonHistogram(expected_reasons);
}

// Regression test for crbug.com/1441276. Sequence of events:
// 1. Loads http://example.com. This gets upgraded to https://example.com.
// 2. https://example.com has an iframe for https://nonexistentsite.com. It
//    navigates away immediately to http://example.com.
// 3. This causes a crash in
//    HttpsUpgradesInterceptor::MaybeCreateLoaderForResponse() for
//    nonexistentsite.com.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       LoadIFrameAndNavigateAway_ShouldNotCrash) {
  // This test is only interesting for HTTPS-Upgrades and HTTPS-First Mode.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }

  // Disable the testing port configuration, as this test doesn't use the
  // EmbeddedTestServer.
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(0);
  HttpsUpgradesInterceptor::SetHttpPortForTesting(0);

  bool navigated_once = false;
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&navigated_once](
              content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url == GURL("https://example.com")) {
              if (!navigated_once) {
                // Load an iframe that will result in an error and immediately
                // navigate away.
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                    "<html>"
                    "<iframe src='https://nonexistentsite.com'></iframe>"
                    "<script>window.location.href = "
                    "'http://example.com';</script></html>",
                    params->client.get());
                navigated_once = true;
                return true;
              }
              // Return a normal response the second time this is called,
              // otherwise the test will timeout due to navigating back and
              // forth between http and https URLs.
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Done</html>", params->client.get());
              return true;
            }

            if (params->url_request.url == GURL("http://example.com")) {
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html>Test</html>", params->client.get());
              return true;
            }

            if (params->url_request.url.host() == "nonexistentsite.com") {
              // This request must fail for the bug to trigger.
              params->client->OnComplete(network::URLLoaderCompletionStatus(
                  net::ERR_CONNECTION_RESET));
              return true;
            }
            return false;
          }));

  GURL http_url("http://example.com");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
}

// If the user triggers an HTTPS-Only Mode interstitial for a host and then
// clicks through the interstitial, they should end up on the HTTP URL.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       InterstitialBypassed_HttpFallbackLoaded) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceed through the interstitial, which will add the host to the
    // allowlist and navigate to the HTTP fallback URL.
    ProceedThroughInterstitial(contents);

    // Verify that the interstitial metrics were correctly recorded.
    histograms()->ExpectTotalCount("interstitial.https_first_mode.decision", 2);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // Verify that navigation event metrics were correctly recorded.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted,
                                    1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError,
                                    1);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }
}

// If the upgraded HTTPS URL is not available due to a net error, it should
// trigger the HTTPS-Only Mode interstitial and offer fallback.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NetErrorOnUpgrade_ShouldInterstitial) {
  // This test is only interesting when HTTPS Upgrading occurs.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeNetError, 1);
}

// If the upgraded HTTPS URL is not available due to a potentially-transient
// exempted net error (here a DNS resolution error), show the regular net error
// page instead of the HTTPS-First Mode interstitial. If the network conditions
// change such that the network error no longer triggers, reloading the tab
// should continue the upgraded navigation, which will fail and trigger fallback
// to HTTP. (Regression test for crbug.com/1277211.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       ExemptNetErrorOnUpgrade_ShouldNotFallback) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  {
    // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED. Navigating
    // to the HTTP URL should get upgraded to HTTPS, but fail with a net error
    // page on the HTTPS URL.
    auto dns_failure_interceptor =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              params->client->OnComplete(network::URLLoaderCompletionStatus(
                  net::ERR_NAME_NOT_RESOLVED));
              return true;
            }));
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));

    // Reload the tab. The net error should still be showing as the navigation
    // still results in ERR_NAME_NOT_RESOLVED.
    content::TestNavigationObserver nav_observer(contents, 1);
    contents->GetController().Reload(content::ReloadType::NORMAL,
                                     /*check_for_repost=*/false);
    nav_observer.Wait();
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  }

  // Interceptor is now out of scope and no longer applies. Reload the tab and
  // the upgraded navigation should continue, fail due to the bad HTTPS on the
  // server, and fall back to HTTP.
  content::TestNavigationObserver nav_observer(contents, 1);
  contents->GetController().Reload(content::ReloadType::NORMAL,
                                   /*check_for_repost=*/false);
  nav_observer.Wait();

  if (IsHttpsFirstModePrefEnabled()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  // Should now be on the HTTP URL and it should be allowlisted.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Test that if one site redirects to a non-existent site, that we show the
// regular net error page instead of the HTTPS-First Mode interstitial.
// (Regression test for crbug.com/1277211.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       RedirectToNonexistentSite_ShouldNotInterstitial) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  std::string nonexistent_domain = "nonexistentsite.com";
  GURL nonexistent_http_url =
      http_server()->GetURL(nonexistent_domain, "/simple.html");
  GURL nonexistent_https_url =
      https_server()->GetURL(nonexistent_domain, "/simple.html");
  std::string www_redirect_path =
      base::StrCat({"/server-redirect?", nonexistent_http_url.spec()});
  GURL redirecting_https_url =
      https_server()->GetURL("foo.com", www_redirect_path);
  GURL redirecting_http_url =
      http_server()->GetURL("foo.com", www_redirect_path);

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED for
  // nonexistentsite.com.
  auto dns_failure_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(
          base::BindLambdaForTesting(
              [nonexistent_domain](
                  content::URLLoaderInterceptor::RequestParams* params) {
                if (params->url_request.url.host() == nonexistent_domain) {
                  params->client->OnComplete(network::URLLoaderCompletionStatus(
                      net::ERR_NAME_NOT_RESOLVED));
                  return true;
                }
                return false;
              }));

  // Navigating to the HTTP URL should get upgraded to HTTPS, but fail with a
  // net error page on the HTTPS URL.
  EXPECT_FALSE(content::NavigateToURL(contents, redirecting_http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
  EXPECT_EQ(url::kHttpsScheme, contents->GetLastCommittedURL().scheme());
  EXPECT_EQ(nonexistent_domain, contents->GetLastCommittedURL().host());
}

// If the upgraded HTTPS URL is not available due to a potentially-transient
// exempted net error but the hostname is non-unique, don't show the net error
// page and instead just fallback to HTTP and the HTTPS-First Mode interstitial.
// Otherwise, the user can be stuck on the net error page when the HTTP version
// of the host would have resolved, such as for corp single-label hostnames.
// (Regression test for crbug.com/1451040.)
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    ExemptNetErrorOnUpgrade_NonUniqueHostname_ShouldFallback) {
  // This test is only interesting when HTTPS-First Mode is enabled.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("blorp", "/simple.html");
  GURL https_url = https_server()->GetURL("blorp", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Set up an interceptor that will return ERR_NAME_NOT_RESOLVED. Navigating
  // to the HTTP URL should get upgraded to HTTPS, and then fallback to HTTP
  // and the HFM interstitial.
  auto dns_failure_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          [](content::URLLoaderInterceptor::RequestParams* params) {
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_NAME_NOT_RESOLVED));
            return true;
          }));
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
  ProceedThroughInterstitial(contents);

  // Should now be on the HTTP URL and it should be allowlisted.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Navigations in subframes should not get upgraded by HTTPS-Only Mode. They
// should be blocked as mixed content.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsParentHttpSubframeNavigation_Blocked) {
  const GURL parent_url(
      https_server()->GetURL("foo.com", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("foo.com", "/simple.html"));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(contents, parent_url));

  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_NE(iframe_url, nav_observer.last_navigation_url());

  // Verify that no navigation event metrics were recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 0);
}

// Navigating to an HTTP URL in a subframe of an HTTP page should not upgrade
// the subframe navigation to HTTPS (even if the subframe navigation is to a
// different host than the parent frame).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpParentHttpSubframeNavigation_NotUpgraded) {
  // The parent frame will fail to upgrade to HTTPS.
  const GURL parent_url(
      http_server()->GetURL("bad-https.com", "/iframe_blank.html"));
  const GURL iframe_url(http_server()->GetURL("bar.com", "/simple.html"));

  // Navigate to `parent_url` and bypass the HTTPS-Only Mode warning.
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, parent_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    // Proceeding through the interstitial will add the hostname to the
    // allowlist.
    ProceedThroughInterstitial(contents);
  }

  // Verify that navigation event metrics were recorded for the main frame.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }

  // Navigate the iframe to `iframe_url`. It should successfully navigate and
  // not get upgraded to HTTPS as the hostname is now in the allowlist.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_TRUE(content::NavigateIframeToURL(contents, "test", iframe_url));
  nav_observer.Wait();
  EXPECT_EQ(iframe_url, nav_observer.last_navigation_url());

  // Verify that no new navigation event metrics were recorded for the subframe.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }
}

// Tests that a navigation to the HTTP version of a site with an HTTPS version
// that is slow to respond gets upgraded to HTTPS but times out and shows the
// HTTPS-Only Mode interstitial.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, SlowHttps_ShouldInterstitial) {
  // Set timeout to zero so that HTTPS upgrades immediately timeout.
  HttpsUpgradesNavigationThrottle::set_timeout_for_testing(0);

  // Set up a custom HTTPS server that times out without sending a response.
  net::EmbeddedTestServer timeout_server{net::EmbeddedTestServer::TYPE_HTTPS};
  timeout_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Server will hang until destroyed.
        return std::make_unique<net::test_server::HungResponse>();
      }));
  ASSERT_TRUE(timeout_server.Start());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(timeout_server.port());

  const GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
}

// Tests that an HTTP POST form navigation to "bar.com" from an HTTP page on
// "foo.com" is not upgraded to HTTPS. (HTTP form navigations from HTTPS are
// blocked by the Mixed Forms warning.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, HttpPageHttpPost_NotUpgraded) {
  // Point the HTTP form target to "bar.com".
  base::StringPairs replacement_text;
  replacement_text.emplace_back(make_pair(
      "REPLACE_WITH_HOST_AND_PORT",
      net::HostPortPair::FromURL(http_server()->GetURL("foo.com", "/"))
          .ToString()));
  auto replacement_path = net::test_server::GetFilePathWithReplacements(
      "/ssl/page_with_form_targeting_http_url.html", replacement_text);

  // Navigate to the page hosting the form on "foo.com".
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigateToURLBlockUntilNavigationsComplete(
      contents, http_server()->GetURL("bad-https.com", replacement_path), 1);

  if (IsHttpsFirstModePrefEnabled()) {
    // The HTTPS-Only Mode interstitial should trigger.
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    // Proceed through the interstitial to add the hostname to the allowlist.
    ProceedThroughInterstitial(contents);
  }

  // Verify that navigation event metrics were recorded for the initial page.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }

  // Submit the form and wait for the navigation to complete.
  content::TestNavigationObserver nav_observer(contents, 1);
  ASSERT_TRUE(
      content::ExecJs(contents, "document.getElementById('submit').click();"));
  nav_observer.Wait();

  // Check that the navigation has ended up on the HTTP target.
  EXPECT_EQ("foo.com", contents->GetLastCommittedURL().host());
  EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpScheme));

  // Verify that no new navigation event metrics were recorded for the POST
  // navigation.
  if (IsHttpUpgradingEnabled()) {
    histograms()->ExpectTotalCount(kEventHistogram, 3);
  } else {
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }
}

// Tests that if an HTTPS navigation redirects to HTTP on a different host, it
// should upgrade to HTTPS on that new host. (A downgrade redirect on the same
// host would imply a redirect loop.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsToHttpRedirect_ShouldUpgrade) {
  GURL target_url = http_server()->GetURL("bar.com", "/title1.html");
  GURL url = https_server()->GetURL("foo.com",
                                    "/server-redirect?" + target_url.spec());

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // NavigateToURL() returns `false` because the final redirected URL does not
  // match `url`. Separately ensure the navigation succeeded using a navigation
  // observer.
  content::TestNavigationObserver nav_observer(contents, 1);
  EXPECT_FALSE(content::NavigateToURL(contents, url));
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());

  // Verify that navigation event metrics were correctly recorded.
  if (IsHttpUpgradingEnabled()) {
    EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpsScheme));
    histograms()->ExpectTotalCount(kEventHistogram, 2);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted,
                                    1);
    histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeSucceeded,
                                    1);
  } else {
    EXPECT_TRUE(contents->GetLastCommittedURL().SchemeIs(url::kHttpScheme));
    histograms()->ExpectTotalCount(kEventHistogram, 1);
    histograms()->ExpectBucketCount(kEventHistogram,
                                    Event::kUpgradeNotAttempted, 1);
  }

  EXPECT_EQ("bar.com", contents->GetLastCommittedURL().host());
}

// Tests that navigating to an HTTPS page that downgrades to HTTP on the same
// host will fail and trigger the HTTPS-Only Mode interstitial (due to
// interceptor detecting a redirect loop and triggering fallback).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       RedirectLoop_ShouldInterstitial) {
  // This test is only interesting if some form of HTTPS upgrading is enabled.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }

  // Set up a new test server instance so it can have a custom handler.
  net::EmbeddedTestServer downgrading_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Downgrade by swapping the scheme for HTTP. HTTPS-Only Mode will upgrade it
  // back to HTTPS.
  downgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL::Replacements http_downgrade;
        http_downgrade.SetSchemeStr(url::kHttpScheme);
        // The HttpRequest will by default refer to the test server by the
        // loopback address rather than any hostname in the navigation (i.e.,
        // the EmbeddedTestServer has no notion of virtual hosts). This
        // explicitly sets the hostname back to the test host so that this
        // doesn't fail due to the exception for localhost.
        http_downgrade.SetHostStr("foo.com");
        auto redirect_url = request.GetURL().ReplaceComponents(http_downgrade);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  ASSERT_TRUE(downgrading_server.Start());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(downgrading_server.port());

  GURL url = downgrading_server.GetURL("foo.com", "/");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }

  // Verify that navigation event metrics were correctly recorded.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeRedirectLoop,
                                  1);
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a net error on HTTPS. (Without HTTPS-Only Mode, a
// net error would be a security level of NONE.)
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       NetErrorOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

    // Proceed through the interstitial to navigate to the HTTP site.
    ProceedThroughInterstitial(contents);
  }

  // The HTTP site results in a net error, which should have security level NONE
  // (as no connection was made).
  // TODO(crbug.com/1394910): Uncomment once upgrades are tracked
  // per-navigation.
  // EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
}

// Tests that the security level is WARNING when the HTTPS-Only Mode
// interstitial is shown for a cert error on HTTPS. (Without HTTPS-Only Mode, a
// a cert error would be a security level of DANGEROUS.) After clicking through
// the interstitial, the security level should still be WARNING.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       BrokenSSLOnUpgrade_SecurityLevelWarning) {
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());

    // Proceed through the interstitial to navigate to the HTTP page.
    ProceedThroughInterstitial(contents);
  }

  // The security level should still be WARNING.
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
}

// Regression test for crbug.com/1233207.
// Tests the case where the HTTP version of a site redirects to HTTPS, but the
// HTTPS version of the site has a cert error. If the user initially navigates
// to the HTTP URL, then HTTPS-First Mode should upgrade the navigation to HTTPS
// and trigger the HTTPS-First Mode interstitial when that fails, but if the
// user clicks through the HTTPS-First Mode interstitial and falls back into the
// HTTP->HTTPS redirect back to the cert error, then the SSL interstitial should
// be shown and the user should be able to click through the SSL interstitial to
// visit the HTTPS version of the site (but in a DANGEROUS security level
// state).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       HttpsUpgradeWithBrokenSSL_ShouldTriggerSSLInterstitial) {
  // If HTTPS upgrading isn't enabled, skip over this test.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }
  // Set up a new test server instance so it can have a custom handler that
  // redirects to the HTTPS server.
  net::EmbeddedTestServer upgrading_server{net::EmbeddedTestServer::TYPE_HTTP};
  upgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader(
            "Location",
            "https://bad-https.com:" +
                base::NumberToString(
                    HttpsUpgradesInterceptor::GetHttpsPortForTesting()) +
                "/simple.html");
        return response;
      }));
  HttpsUpgradesInterceptor::SetHttpPortForTesting(upgrading_server.port());
  ASSERT_TRUE(upgrading_server.Start());

  GURL http_url = upgrading_server.GetURL("bad-https.com", "/simple.html");
  // HTTPS server will have a cert error.
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    // The HTTPS-First Mode interstitial should trigger first.
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));

    // Proceeding through the HTTPS-First Mode interstitial will hit the
    // upgrading server's HTTP->HTTPS redirect. This should result in an SSL
    // interstitial (not an HTTPS-First Mode interstitial).
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Proceeding through the SSL interstitial should navigate to the HTTPS
  // version of the site but with the DANGEROUS security level.
  ProceedThroughInterstitial(contents);
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());

  // Verify that navigation event metrics were correctly recorded. They should
  // only have been recorded for the initial navigation that resulted in the
  // HTTPS-First Mode interstitial.
  histograms()->ExpectTotalCount(kEventHistogram, 3);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeAttempted, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeFailed, 1);
  histograms()->ExpectBucketCount(kEventHistogram, Event::kUpgradeCertError, 1);

  if (IsHttpsFirstModePrefEnabled()) {
    // Verify that the interstitial metrics were correctly recorded.
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::SHOW, 1);
    histograms()->ExpectBucketCount(
        "interstitial.https_first_mode.decision",
        security_interstitials::MetricsHelper::Decision::PROCEED, 1);
  }
}

// Tests that clicking the "Learn More" link in the HTTPS-First Mode
// interstitial opens a new tab for the help center article.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, InterstitialLearnMoreLink) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the learn more link (CMD_OPEN_HELP_CENTER).
  ASSERT_TRUE(content::ExecJs(
      contents, "window.certificateErrorPageController.openHelpCenter();"));

  // New tab should include the p-link "first_mode".
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .query(),
            "p=first_mode");

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.interaction",
      security_interstitials::MetricsHelper::Interaction::TOTAL_VISITS, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.interaction",
      security_interstitials::MetricsHelper::Interaction::SHOW_LEARN_MORE, 1);
}

// Tests that if the user bypasses the HTTPS-First Mode interstitial, and then
// later the server fixes their HTTPS support and the user successfully connects
// over HTTPS, the allowlist entry is cleared (so HFM will kick in again for
// that site).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, BadHttpsFollowedByGoodHttps) {
  // TODO(crbug.com/1394910): This test is flakey when only HTTPS Upgrades are
  // enabled.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL bad_https_url = https_server()->GetURL("foo.com", "/close-socket");
  GURL good_https_url = https_server()->GetURL("foo.com", "/ssl/google.html");

  ASSERT_EQ(http_url.host(), bad_https_url.host());
  ASSERT_EQ(bad_https_url.host(), good_https_url.host());

  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = static_cast<StatefulSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());

  // First check that main frame requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  NavigateAndWaitForFallback(tab, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
    ProceedThroughInterstitial(tab);
  }

  EXPECT_TRUE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_TRUE(content::NavigateToURL(tab, good_https_url));
  EXPECT_FALSE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Rarely, an open connection with the bad cert might be reused for the next
  // navigation, which is supposed to show an interstitial. Close open
  // connections to ensure a fresh connection (and certificate validation) for
  // the next navigation. See https://crbug.com/1150592. A deeper fix for this
  // issue would be to unify certificate bypass logic which is currently split
  // between the net stack and content layer; see https://crbug.com/488043.
  // See also: SSLUITest.BadCertFollowedByGoodCert.
  state->RevokeUserAllowExceptionsHard(http_url.host());

  // Now check that subresource requests revoke the decision.

  // Navigate to `http_url`, which will get upgraded to `bad_https_url`.
  NavigateAndWaitForFallback(tab, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(tab));
    ProceedThroughInterstitial(tab);
  }

  EXPECT_TRUE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Load "logo.gif" as an image on the page.
  GURL image = https_server()->GetURL("foo.com", "/ssl/google_files/logo.gif");

  EXPECT_EQ(
      true,
      EvalJs(tab,
             std::string("var img = document.createElement('img');img.src ='") +
                 image.spec() +
                 "';"
                 "new Promise(resolve => {"
                 "  img.onload=function() { "
                 "    resolve(true); };"
                 "  document.body.appendChild(img);"
                 "});"));

  EXPECT_FALSE(state->HasAllowException(
      http_url.host(), tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests that clicking the "Go back" button in the HTTPS-First Mode interstitial
// navigates back to the previous page (about:blank in this case).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, InterstitialGoBack) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the "Go back" button.
  DontProceedThroughInterstitial(contents);

  EXPECT_EQ(GURL("about:blank"), contents->GetLastCommittedURL());

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
}

// Tests that closing the tab of the HTTPS-First Mode interstitial counts as
// not proceeding through the interstitial for metrics.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, CloseInterstitialTab) {
  // This test is only relevant to HTTPS-First Mode.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL http_url = http_server()->GetURL("foo.com", "/close-socket");
  GURL https_url = https_server()->GetURL("foo.com", "/close-socket");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Leave the interstitial by closing the tab.
  chrome::CloseWebContents(browser(), contents, false);

  // Verify that the interstitial metrics were correctly recorded.
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::SHOW, 1);
  histograms()->ExpectBucketCount(
      "interstitial.https_first_mode.decision",
      security_interstitials::MetricsHelper::Decision::DONT_PROCEED, 1);
}

// Tests that if a user allowlists a host and then does not visit it again for
// seven days (the expiration period), then the interstitial will be shown again
// the next time they visit the host.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, AllowlistEntryExpires) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set a testing clock on the StatefulSSLHostStateDelegate, keeping a pointer
  // to the clock object around so the test can manipulate time. `chrome_state`
  // takes ownership of `clock`.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // Visit a host that doesn't support HTTPS for the first time, and click
  // through the HTTPS-First Mode interstitial to allowlist the host.
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  if (IsHttpUpgradingEnabled()) {
    EXPECT_TRUE(state->IsHttpAllowedForHost(
        http_url.host(),
        contents->GetPrimaryMainFrame()->GetStoragePartition()));
  }

  // Simulate the clock advancing by eight days, which is past the expiration
  // point.
  clock_ptr->Advance(base::Days(8));

  // The host should no longer be allowlisted, and the interstitial should
  // trigger again.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
}

// Tests that re-visiting an allowlisted host bumps the expiration time to a new
// seven days in the future from now.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, RevisitingBumpsExpiration) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Set a testing clock on the StatefulSSLHostStateDelegate, keeping a pointer
  // to the clock object around so the test can manipulate time. `chrome_state`
  // takes ownership of `clock`.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // Visit a host that doesn't support HTTPS for the first time, and click
  // through the HTTPS-First Mode interstitial to allowlist the host.
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  NavigateAndWaitForFallback(contents, http_url);

  if (IsHttpsFirstModePrefEnabled()) {
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
    ProceedThroughInterstitial(contents);
  }

  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  // If upgrading isn't enabled, ensure the host wasn't allowlisted and exit.
  if (!IsHttpUpgradingEnabled()) {
    EXPECT_FALSE(state->IsHttpAllowedForHost(
        http_url.host(),
        contents->GetPrimaryMainFrame()->GetStoragePartition()));
    return;
  }

  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the clock advancing by five days.
  clock_ptr->Advance(base::Days(5));

  // Navigate to the host again; this will reset the allowlist expiration to
  // now + 7 days.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));

  // Simulate the clock advancing another five days. This will be _after_ the
  // initial expiration date of the allowlist entry, but _before_ the bumped
  // expiration date from the second navigation.
  clock_ptr->Advance(base::Days(5));
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if a hostname has an HSTS entry registered, then HTTPS-First Mode
// should not try to upgrade it (instead allowing HSTS to handle the upgrade as
// it is more strict).
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, PreferHstsOverHttpsFirstMode) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // URL for HTTPS server that will result in a certificate error.
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  // HTTP version of that URL that will get upgraded to HTTPS (but with the
  // correct port for the HTTPS server -- the test code can configure
  // HTTPS-First Mode to be aware of the different ports, but can't do that for
  // HSTS).
  GURL::Replacements downgrade_scheme_to_http;
  downgrade_scheme_to_http.SetSchemeStr(url::kHttpScheme);
  GURL http_url = https_url.ReplaceComponents(downgrade_scheme_to_http);

  // Add hostname to the TransportSecurityState.
  base::Time expiry = base::Time::Now() + base::Days(100);
  bool include_subdomains = false;
  auto* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  base::RunLoop run_loop;
  network_context->AddHSTS(http_url.host(), expiry, include_subdomains,
                           run_loop.QuitClosure());
  run_loop.Run();

  // Navigate to the HTTP URL. It should get upgraded to HTTPS and trigger a
  // fatal certificate error (because of HTTPS) instead of falling back to the
  // HTTPS-First Mode interstitial.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingSSLInterstitial(contents));

  // Verify that no HFM event histograms were emitted (to check that HFM did not
  // trigger for this navigation at all).
  histograms()->ExpectTotalCount(kEventHistogram, 0);

  // Verify that general navigation request metrics were recorded.
  histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 2);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kHstsUpgraded,
                                  1);
  histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                  NavigationRequestSecurityLevel::kSecure, 1);
}

// Regression test for crbug.com/1272781. Previously, performing back/forward
// navigations around the HTTPS-First Mode interstitial could cause history
// entries to dropped.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       InterstitialFallbackMaintainsHistory) {
  // This test only applies to HTTPS-First Mode.
  if (!IsHttpsFirstModePrefEnabled()) {
    return;
  }

  GURL good_https_url = https_server()->GetURL("site1.com", "/defaultresponse");

  // Set up a new test server instance so it can have a custom handler.
  net::EmbeddedTestServer downgrading_server{
      net::EmbeddedTestServer::TYPE_HTTPS};
  // Downgrade by swapping the scheme for HTTP. HTTPS-First Mode will upgrade it
  // back to HTTPS.
  downgrading_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL::Replacements http_downgrade;
        http_downgrade.SetSchemeStr(url::kHttpScheme);
        // The HttpRequest will by default refer to the test server by the
        // loopback address rather than any hostname in the navigation (i.e.,
        // the EmbeddedTestServer has no notion of virtual hosts). This
        // explicitly sets the hostname back to the test host so that this
        // doesn't fail due to the exception for localhost.
        http_downgrade.SetHostStr("site2.com");
        auto redirect_url = request.GetURL().ReplaceComponents(http_downgrade);
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        response->AddCustomHeader("Location", redirect_url.spec());
        return response;
      }));
  ASSERT_TRUE(downgrading_server.Start());
  HttpsUpgradesInterceptor::SetHttpsPortForTesting(downgrading_server.port());

  GURL downgrading_https_url = downgrading_server.GetURL("site2.com", "/");
  GURL::Replacements swap_http_scheme;
  swap_http_scheme.SetSchemeStr(url::kHttpScheme);
  GURL downgrading_http_url =
      downgrading_https_url.ReplaceComponents(swap_http_scheme);

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a "good" HTTPS site.
  EXPECT_TRUE(content::NavigateToURL(contents, good_https_url));

  // Navigate to the HTTP version of `downgrading_https_url`, which will get
  // upgraded to HTTPS and fail, triggering the HTTPS-First Mode
  // interstitial.
  content::NavigateToURLBlockUntilNavigationsComplete(contents,
                                                      downgrading_http_url, 1);
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Simulate clicking the browser "back" button.
  EXPECT_TRUE(content::HistoryGoBack(contents));
  EXPECT_EQ(good_https_url, contents->GetLastCommittedURL());
  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  EXPECT_EQ(security_state::SECURE, helper->GetSecurityLevel());

  // Simulate clicking the browser "forward" button. The HistoryGoForward()
  // call returns `false` because it is an error page.
  EXPECT_FALSE(content::HistoryGoForward(contents));
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // No forward entry should be present.
  EXPECT_FALSE(contents->GetController().CanGoForward());

  // Simulate clicking the browser "back" button again. Previously this would
  // result in `about:blank` being shown.
  EXPECT_TRUE(content::HistoryGoBack(contents));
  EXPECT_EQ(good_https_url, contents->GetLastCommittedURL());

  // Repeat forward one last time. (Previously the user would no longer be able
  // to go back any more as the history entries were lost.)
  EXPECT_FALSE(content::HistoryGoForward(contents));  // error page -> false
  EXPECT_EQ(downgrading_http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  EXPECT_TRUE(contents->GetController().CanGoBack());
}

// Tests that if the HttpAllowlist enterprise policy is set, then HTTPS upgrades
// are skipped for hosts in the allowlist. Includes simple hostname, wildcard
// hostname pattern, and bare IP address cases.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       EnterpriseAllowlistDisablesUpgrades) {
  // Skip this test when HTTPS Upgrading isn't enabled.
  if (!IsHttpUpgradingEnabled()) {
    return;
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Without any policy allowlist, navigate to HTTP URL on foo.com. It *should*
  // get upgraded to HTTPS.
  auto http_url = http_server()->GetURL("foo.com", "/simple.html");
  auto https_url = https_server()->GetURL("foo.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Artificially add the pref that gets mapped from the enterprise policy.
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* prefs = profile->GetPrefs();
  base::Value::List allowlist;
  allowlist.Append("foo.com");
  allowlist.Append("[*.]bar.com");
  allowlist.Append(http_server()->GetIPLiteralString());
  // These cases should not work, but the policy->pref mapping won't immediately
  // reject them.
  allowlist.Append("[*]");
  allowlist.Append("*");
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowlist));

  // Navigate to HTTP URL on foo.com. It should not get upgraded to HTTPS and
  // no interstitial should be shown.
  http_url = http_server()->GetURL("foo.com", "/simple.html");
  https_url = https_server()->GetURL("foo.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.com. Same result.
  http_url = http_server()->GetURL("bar.com", "/simple.html");
  https_url = https_server()->GetURL("bar.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on bar.bar.com. Same result as subdomain wildcard
  // was specified.
  http_url = http_server()->GetURL("bar.bar.com", "/simple.html");
  https_url = https_server()->GetURL("bar.bar.com", "/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Navigate to HTTP URL on foo.foo.com. Subdomains of foo.com should not be
  // considered as being in the allowlist as no wildcard was specified. This
  // should get upgraded to HTTPS.
  http_url = http_server()->GetURL("foo.foo.com", "/simple.html");
  https_url = https_server()->GetURL("foo.foo.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on baz.com, which is not on the allowlist. Should get
  // upgraded to HTTPS.
  http_url = http_server()->GetURL("baz.com", "/simple.html");
  https_url = https_server()->GetURL("baz.com", "/simple.html");
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(https_url, contents->GetLastCommittedURL());

  // Navigate to HTTP URL on the HTTP test server's IP address. It should not
  // get upgraded to HTTPS and no interstitial should be shown.
  http_url = http_server()->GetURL("/simple.html");
  https_url = https_server()->GetURL("/simple.html");
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

// Tests that if the HttpAllowlist enterprise policy is set, then HTTPS upgrades
// and HTTPS-First Mode For Site Engagement checks are skipped for hosts in the
// allowlist.
IN_PROC_BROWSER_TEST_P(
    HttpsUpgradesBrowserTest,
    EnterpriseAllowlistDisablesHttpsFirstModeForSiteEngagament) {
  // Skip this test when HTTPS-First Mode for Site Engagement isn't enabled.
  if (!IsSiteEngagementHeuristicEnabled()) {
    return;
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Without any policy allowlist, navigate to an HTTP URL. It should show the
  // HFM+SE interstitial.
  auto http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  auto https_url = https_server()->GetURL("bad-https.com", "/simple.html");
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);

  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));

  // Artificially add the pref that gets mapped from the enterprise policy.
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* prefs = profile->GetPrefs();
  base::Value::List allowlist;
  allowlist.Append("bad-https.com");
  prefs->SetList(prefs::kHttpAllowlist, std::move(allowlist));

  // Navigate to the same URL. It should not get upgraded to HTTPS and
  // no interstitial should be shown.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));
}

IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       EnterprisePolicyDisablesUpgrades) {
  // Disable HTTPS-Upgrades via enterprise policy.
  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHttpsUpgradesEnabled, false);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");

  if (IsHttpsFirstModePrefEnabled()) {
    // HTTPS-First Mode should supercede HTTPS-Upgrades and upgrade the
    // navigation despite the HttpsUpgradeMode policy setting.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else if (IsHttpUpgradingEnabled()) {
    // If HTTPS-First Mode is not enabled but upgrading is, then the policy
    // should prevent the upgrade.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }
}

// Test that HTTPS Upgrades are skipped if the "Insecure content" site setting
// is set to "allow".
// MIXED_SCRIPT isn't enabled as a content setting on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InsecureContentSettingDisablesUpgrades \
  DISABLED_InsecureContentSettingDisablesUpgrades
#else
#define MAYBE_InsecureContentSettingDisablesUpgrades \
  InsecureContentSettingDisablesUpgrades
#endif
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       MAYBE_InsecureContentSettingDisablesUpgrades) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Set insecure content setting to allowed for `http_url`.
  host_content_settings_map->SetContentSettingDefaultScope(
      http_url, GURL(), ContentSettingsType::MIXEDSCRIPT,
      CONTENT_SETTING_ALLOW);

  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }

  // Unset the content settings.
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::MIXEDSCRIPT);

  // Set insecure content setting to allowed for `https_url`.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(https_url, GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);
  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 2);
  }
}

// Test that HTTPS Upgrades are skipped if the "Insecure content" site setting
// is set to "allow".
// MIXED_SCRIPT isn't enabled as a content setting on Android.
// This test is identical to InsecureContentSettingDisablesUpgrades except it
// sets a high site engagement score for the https URL and checks an additional
// histogram.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InsecureContentSettingDisablesHFMForEngagedSites \
  DISABLED_InsecureContentSettingDisablesHFMForEngagedSites
#else
#define MAYBE_InsecureContentSettingDisablesHFMForEngagedSites \
  InsecureContentSettingDisablesHFMForEngagedSites
#endif
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       MAYBE_InsecureContentSettingDisablesHFMForEngagedSites) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  // Setting a high engagement score on the HTTPS URL enables HFM on the site
  // if the HFM+SE feature is enabled, but an Insecure Content entry disables
  // HFM+SE on the site.
  SetSiteEngagementScore(http_url, kLowSiteEngagementScore);
  SetSiteEngagementScore(https_url, kHighSiteEnagementScore);

  // Set insecure content setting to allowed for `http_url`.
  host_content_settings_map->SetContentSettingDefaultScope(
      http_url, GURL(), ContentSettingsType::MIXEDSCRIPT,
      CONTENT_SETTING_ALLOW);

  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    1);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 1);
  }
  // In both cases, HFM+SE events shouldn't be recorded because of the Insecure
  // content setting.
  histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);

  // Unset the content settings.
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::MIXEDSCRIPT);

  // Set insecure content setting to allowed for `https_url`.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(https_url, GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);
  if (IsHttpsFirstModePrefEnabled()) {
    // If HTTPS-First Mode is enabled, upgrades should still be applied.
    EXPECT_FALSE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(kNavigationRequestSecurityLevelHistogram,
                                    NavigationRequestSecurityLevel::kUpgraded,
                                    2);
  } else {
    // Otherwise, the upgrades should be skipped.
    EXPECT_TRUE(content::NavigateToURL(contents, http_url));
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kAllowlisted, 2);
  }
  // In both cases, HFM+SE events shouldn't be recorded because of the Insecure
  // content setting.
  histograms()->ExpectTotalCount(kEventHistogramWithEngagementHeuristic, 0);
}

// Regression test for crbug.com/1431026. Triggers a navigation where HTTPS
// upgrades applied multiple times across redirects to different sites.
// Should not crash when DCHECKS are enabled.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest, crbug1431026) {
  GURL www_bad_https_url =
      https_server()->GetURL("www.bad-https.com", "/simple.html");
  GURL www_http_url =
      http_server()->GetURL("www.bad-https.com", "/simple.html");

  // Configure HTTP and bad-HTTPS URLs which redirect to www. subdomain.
  std::string www_redirect_path =
      base::StrCat({"/server-redirect?", www_http_url.spec()});
  GURL redirecting_bad_https_url =
      https_server()->GetURL("bad-https.com", www_redirect_path);
  GURL redirecting_http_url =
      http_server()->GetURL("bad-https.com", www_redirect_path);

  // A good HTTPS URL which redirects to an HTTP URL, which also redirects.
  GURL initial_redirecting_good_https_url = https_server()->GetURL(
      "good-https.com",
      base::StrCat({"/server-redirect-301?", redirecting_http_url.spec()}));

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      content::NavigateToURL(contents, initial_redirecting_good_https_url));

  if (IsHttpsFirstModePrefEnabled()) {
    // Should be showing interstitial on http://bad-https.com/.
    EXPECT_EQ(redirecting_http_url, contents->GetLastCommittedURL());
    EXPECT_TRUE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  } else {
    // Either due to no upgrades, or due to fast fallback to HTTP, this should
    // end up on http://www.bad-https.com.
    EXPECT_EQ(www_http_url, contents->GetLastCommittedURL());
    EXPECT_FALSE(
        chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
            contents));
  }
}

// Tests that when the HTTPS-First Mode setting is toggled on or off, the
// HTTP allowlist is cleared.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       TogglingSettingClearsAllowlist) {
  auto http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Start by enabling HTTPS-First Mode.
  SetPref(true);

  // Navigate to a URL that will fail upgrades, and click through the
  // interstitial to add it to the allowlist.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
  ProceedThroughInterstitial(contents);

  // Disable the HTTPS-First Mode pref. This should clear the allowlist.
  SetPref(false);

  // If HTTPS-Upgrades are enabled, navigating again should cause the site to
  // get added back to the allowlist. If not, the HTTP URL will load normally as
  // HTTPS-First Mode is disabled.
  EXPECT_TRUE(content::NavigateToURL(contents, http_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Re-enable the HTTPS-First Mode pref. The allowlist should be cleared again.
  SetPref(true);

  // Navigate to a URL that will fail upgrades, and the interstitial should be
  // shown again as the allowlist was cleared.
  EXPECT_FALSE(content::NavigateToURL(contents, http_url));
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
      contents));
}

// Tests that URLs typed with an explicit http:// scheme are opted out from
// upgrades.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsTypedWithHttpSchemeNoUpgrades) {
  if (!IsHttpUpgradingEnabled()) {
    return;
  }
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = browser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  // Simulate the full URL was typed with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks(), false, true, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  if (IsHttpsFirstModePrefEnabled()) {
    // Typed http URLs don't opt out of upgrades in HFM.
    EXPECT_EQ(https_url, contents->GetLastCommittedURL());
  } else {
    histograms()->ExpectTotalCount(kNavigationRequestSecurityLevelHistogram, 1);
    histograms()->ExpectBucketCount(
        kNavigationRequestSecurityLevelHistogram,
        NavigationRequestSecurityLevel::kExplicitHttpScheme, 1);
    EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  }
}

// Tests that URLs with an explicit http:// scheme are upgraded if they were
// autocompleted.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsAutocompletedWithHttpSchemeAreUpgraded) {
  if (!IsHttpUpgradingEnabled()) {
    return;
  }
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = browser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  // Simulate the full URL was autocompleted with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::NAVSUGGEST,
      base::TimeTicks(), false, false, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  EXPECT_EQ(https_url, contents->GetLastCommittedURL());
}

// Tests that URLs typed with an explicit http:// scheme that result in an
// opt-out cause the url to be added to the allowlist.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       URLsTypedWithHttpSchemeNoUpgradesAllowlist) {
  if (!IsHttpUpgradingEnabled() || IsHttpsFirstModePrefEnabled()) {
    return;
  }
  GURL http_url = http_server()->GetURL("foo.com", "/simple.html");
  GURL https_url = https_server()->GetURL("foo.com", "/simple.html");
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  OmniboxClient* omnibox_client = browser()
                                      ->window()
                                      ->GetLocationBar()
                                      ->GetOmniboxView()
                                      ->controller()
                                      ->client();

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Site should not yet be in the allowlist.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));

  // Simulate the full URL was typed with an http scheme.
  content::TestNavigationObserver nav_observer(contents, 1);
  omnibox_client->OnAutocompleteAccept(
      http_url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      base::TimeTicks(), false, true, std::u16string(), AutocompleteMatch(),
      AutocompleteMatch(), IDNA2008DeviationCharacter::kNone);
  nav_observer.Wait();

  // URL should not have been upgraded, and site should now be in the allowlist.
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      http_url.host(), contents->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Regression test for crbug.com/1475747. With HTTPS-Upgrades enabled but
// HFM-for-Typically-Secure-Users disabled, sets the prefs as though
// HFM-for-Typically-Secure-Users had triggered accidentally and tests whether
// the remediation that resets this works correctly.
IN_PROC_BROWSER_TEST_P(HttpsUpgradesBrowserTest,
                       AccidentalTypicallySecureUsersRemediation) {
  // Just test for HTTPS-Upgrades (we want HFM-for-Typically-Secure-Users to be
  // disabled).
  if (https_upgrades_test_type() != HttpsUpgradesTestType::kHttpsUpgradesOnly) {
    return;
  }

  // Pretend that the feature had been erroneously enabled previously. The prefs
  // must be set in this order, as setting kHttpsOnlyModeEnabled will cause
  // kHttpsOnlyModeAutoEnabled to be reset to false by the pref observer
  // in HttpsFirstModeService.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled,
                                               true);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeAutoEnabled,
                                               true);

  // Start a navigation to a URL that will be upgraded but fail, which should
  // cause the prefs to be reset and no interstitial should be shown.
  GURL http_url = http_server()->GetURL("bad-https.com", "/simple.html");
  GURL https_url = https_server()->GetURL("bad-https.com", "/simple.html");

  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateAndWaitForFallback(contents, http_url);
  EXPECT_EQ(http_url, contents->GetLastCommittedURL());

  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingHttpsFirstModeInterstitial(
          contents));

  // Check that the prefs have been reset.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kHttpsOnlyModeAutoEnabled));
}

// A simple test fixture that constructs a HistogramTester (so that it gets
// initialized before browser startup). Used for testing pref tracking logic.
class HttpsUpgradesPrefsBrowserTest : public InProcessBrowserTest {
 public:
  HttpsUpgradesPrefsBrowserTest() = default;
  ~HttpsUpgradesPrefsBrowserTest() override = default;

 protected:
  void SetPref(bool enabled) {
    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, enabled);
  }

  bool GetPref() const {
    auto* prefs = browser()->profile()->GetPrefs();
    return prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  }

  base::HistogramTester* histograms() { return &histograms_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
};

// Tests that the HTTPS-First Mode pref is recorded at startup and when changed.
// This test requires restarting the browser to test the "at startup" metric in
// order for the preference state to be set up before the HttpsFirstModeService
// is created.
IN_PROC_BROWSER_TEST_F(HttpsUpgradesPrefsBrowserTest, PRE_PrefStatesRecorded) {
  // The default pref state is `false`, which should get recorded when the
  // initial browser instance is started here.
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", false, 1);

  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Disabled"));

  // Change the pref to true. This should get recorded in the histogram.
  SetPref(true);
  histograms()->ExpectUniqueSample("Security.HttpsFirstMode.SettingChanged",
                                   true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));
}

IN_PROC_BROWSER_TEST_F(HttpsUpgradesPrefsBrowserTest, PrefStatesRecorded) {
  // Restarting the browser from the PRE_ test should record the startup pref
  // histogram. Checking the unique count also ensures that other profile types
  // (e.g. the ChromeOS sign-in profile) don't cause double-counting.
  EXPECT_TRUE(GetPref());
  histograms()->ExpectUniqueSample(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("HttpsFirstModeClientSetting",
                                                  "Enabled"));

  // Open an Incognito window. Startup metrics should not get recorded.
  CreateIncognitoBrowser();
  histograms()->ExpectTotalCount(
      "Security.HttpsFirstMode.SettingEnabledAtStartup", 1);
}
