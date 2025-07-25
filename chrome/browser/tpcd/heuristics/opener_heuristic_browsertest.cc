// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;
using content::WebContentsObserver;
using testing::ElementsAre;
using testing::Field;
using testing::Optional;
using testing::Pair;
using tpcd::experiment::EnableForIframeTypes;

namespace {

struct AccessGrantTestCase {
  bool write_grant_enabled = false;
  bool disable_for_ad_tagged_popups = false;
};

const AccessGrantTestCase kAccessGrantTestCases[] = {
    {.write_grant_enabled = false, .disable_for_ad_tagged_popups = false},
    {.write_grant_enabled = true, .disable_for_ad_tagged_popups = false},
    {.write_grant_enabled = true, .disable_for_ad_tagged_popups = true}};

// Waits for a pop-up to open.
class PopupObserver : public WebContentsObserver {
 public:
  explicit PopupObserver(
      WebContents* web_contents,
      WindowOpenDisposition open_disposition = WindowOpenDisposition::NEW_POPUP)
      : WebContentsObserver(web_contents),
        open_disposition_(open_disposition) {}

  void Wait() { run_loop_.Run(); }
  WebContents* popup() { return popup_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (!popup_ && disposition == open_disposition_) {
      popup_ = new_contents;
      run_loop_.Quit();
    }
  }

  const WindowOpenDisposition open_disposition_;
  raw_ptr<WebContents> popup_ = nullptr;
  base::RunLoop run_loop_;
};

// Waits for a navigation in the primary main frame to finish.
class NavigationFinishObserver : public WebContentsObserver {
 public:
  explicit NavigationFinishObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }

 private:
  // WebContentsObserver overrides:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

}  // namespace

// SubresourceFilterBrowserTest is necessary to test ad-tagging related
// behaviors.
class OpenerHeuristicBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  void SetUp() override {
    tpcd_heuristics_grants_params_["TpcdReadHeuristicsGrants"] = "true";

    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kTpcdHeuristicsGrants,
          tpcd_heuristics_grants_params_}},
        // Disable tracking protection by default to test third-party cookie
        // behavior for PostPopupCookieAccess events.
        {content_settings::features::kTrackingProtection3pcd});

    OpenerHeuristicTabHelper::SetClockForTesting(&clock_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

    // These rules apply an ad-tagging param to scripts in ad_script.js,
    // including `windowOpenFromAdScript`.
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js")});

    DIPSService::Get(GetActiveWebContents()->GetBrowserContext())
        ->SetStorageClockForTesting(&clock_);
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  OpenerHeuristicTabHelper* GetTabHelper() {
    return OpenerHeuristicTabHelper::FromWebContents(GetActiveWebContents());
  }

  DIPSService* GetDipsService() {
    return DIPSService::Get(GetActiveWebContents()->GetBrowserContext());
  }

  void RecordInteraction(const GURL& url, base::Time time) {
    auto* dips = GetDipsService();
    dips->storage()
        ->AsyncCall(&DIPSStorage::RecordInteraction)
        .WithArgs(url, time, dips->GetCookieMode());
    dips->storage()->FlushPostedTasksForTesting();
  }

  // Open a popup window with the given URL and return its WebContents.
  base::expected<WebContents*, std::string> OpenPopup(const GURL& url) {
    auto* web_contents = GetActiveWebContents();
    PopupObserver observer(web_contents);
    if (!content::ExecJs(
            web_contents,
            content::JsReplace("window.open($1, '', 'popup');", url))) {
      return base::unexpected("window.open failed");
    }
    observer.Wait();

    // Wait for the popup to finish navigating to its initial URL.
    NavigationFinishObserver(observer.popup()).Wait();

    // Wait for the read of the past interaction from the DIPS DB to complete,
    // so the PopupPastInteraction UKM event is reported.
    GetDipsService()->storage()->FlushPostedTasksForTesting();

    return observer.popup();
  }

  // Open a popup window with the given URL, using an ad-tagged script, and
  // return its WebContents.
  base::expected<WebContents*, std::string> OpenAdTaggedPopup(const GURL& url) {
    content::WebContentsAddedObserver wca_observer;

    content::ExecuteScriptAsync(
        GetActiveWebContents(),
        content::JsReplace("windowOpenFromAdScript($1)", url));

    content::WebContents* new_web_contents = wca_observer.GetWebContents();
    content::TestNavigationObserver navigation_observer(new_web_contents);
    navigation_observer.Wait();
    if (!navigation_observer.last_navigation_succeeded()) {
      return base::unexpected("windowOpenFromAdScript failed");
    }

    return new_web_contents;
  }

  // Navigate a (possibly nested) iframe of `parent_frame` to `url`. Return true
  // iff navigation is successful.
  [[nodiscard]] bool NavigateIframeTo(content::RenderFrameHost* parent_frame,
                                      const GURL& url) {
    content::TestNavigationObserver load_observer(GetActiveWebContents());
    std::string script = base::StringPrintf(
        "var iframe = document.getElementById('test');iframe.src='%s';",
        url.spec().c_str());
    if (!content::ExecJs(parent_frame, script,
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)) {
      return false;
    }
    load_observer.Wait();
    return load_observer.last_navigation_succeeded();
  }

  void SimulateMouseClick(WebContents* web_contents) {
    content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
    UserActivationObserver observer(web_contents,
                                    web_contents->GetPrimaryMainFrame());
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft);
    observer.Wait();
  }

  void DestroyWebContents(WebContents* web_contents) {
    content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
    web_contents->Close();
    destruction_watcher.Wait();
  }

  base::expected<OptionalBool, std::string> GetOpenerHasSameSiteIframe(
      ukm::TestUkmRecorder& ukm_recorder,
      const std::string& entry_name) {
    std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder.GetEntries(entry_name, {"OpenerHasSameSiteIframe"});
    if (entries.size() != 1) {
      return base::unexpected(
          base::StringPrintf("Expected 1 %s entry, found %zu",
                             entry_name.c_str(), entries.size()));
    }
    return static_cast<OptionalBool>(
        entries[0].metrics["OpenerHasSameSiteIframe"]);
  }

  absl::optional<PopupsStateValue> GetPopupState(const GURL& opener_url,
                                                 const GURL& popup_url) {
    absl::optional<PopupsStateValue> state;

    GetDipsService()
        ->storage()
        ->AsyncCall(&DIPSStorage::ReadPopup)
        .WithArgs(GetSiteForDIPS(opener_url), GetSiteForDIPS(popup_url))
        .Then(base::BindLambdaForTesting(
            [&state](absl::optional<PopupsStateValue> db_state) {
              state = db_state;
            }));
    GetDipsService()->storage()->FlushPostedTasksForTesting();

    return state;
  }

  base::FieldTrialParams tpcd_heuristics_grants_params_;
  base::SimpleTestClock clock_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       RootWindowDoesntHavePopupState) {
  ASSERT_FALSE(GetTabHelper()->popup_observer_for_testing());
}

// TODO(crbug.com/1465642): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithOpenerHavePopupState \
  DISABLED_PopupsWithOpenerHavePopupState
#else
#define MAYBE_PopupsWithOpenerHavePopupState PopupsWithOpenerHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithOpenerHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("window.open($1, '', 'popup');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(https://crbug.com/1469394): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  DISABLED_PopupsWithoutOpenerDoNotHavePopupState
#else
#define MAYBE_PopupsWithoutOpenerDoNotHavePopupState \
  PopupsWithoutOpenerDoNotHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupsWithoutOpenerDoNotHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("window.open($1, '', 'popup,noopener');", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
}

// TODO(crbug.com/1469394): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NewTabURLsHavePopupState DISABLED_NewTabURLsHavePopupState
#else
#define MAYBE_NewTabURLsHavePopupState NewTabURLsHavePopupState
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_NewTabURLsHavePopupState) {
  WebContents* web_contents = GetActiveWebContents();
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(content::ExecJs(
      web_contents, content::JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);
  ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
}

class OpenerHeuristicIframeInitiatorBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<
          ::testing::tuple<EnableForIframeTypes, bool>> {
 public:
  OpenerHeuristicIframeInitiatorBrowserTest()
      : iframe_types_flag_(std::get<0>(GetParam())),
        is_nested_iframe_(std::get<1>(GetParam())) {
    for (const auto [val, str] :
         tpcd::experiment::kEnableForIframeTypesOptions) {
      if (val == iframe_types_flag_) {
        tpcd_heuristics_grants_params_
            ["TpcdPopupHeuristicEnableForIframeInitiator"] = str;
      }
    }
  }

  const EnableForIframeTypes iframe_types_flag_;
  const bool is_nested_iframe_;
};

IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicIframeInitiatorBrowserTest,
    URLsInitiatedByFirstPartyIframes_HavePopupStateWithFlag) {
  WebContents* web_contents = GetActiveWebContents();
  const GURL opener_primary_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const GURL opener_iframe_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, opener_primary_frame_url));
  RenderFrameHost* parent_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_primary_frame_url));
  if (is_nested_iframe_) {
    parent_frame = ChildFrameAt(parent_frame, 0);
    ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_iframe_url));
  }

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(
      content::ExecJs(ChildFrameAt(parent_frame, 0),
                      content::JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);

  if (iframe_types_flag_ == EnableForIframeTypes::kNone) {
    ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
  } else {
    ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
  }
}

IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicIframeInitiatorBrowserTest,
    URLsInitiatedByThirdPartyIframes_HavePopupStateWithFlag) {
  WebContents* web_contents = GetActiveWebContents();
  const GURL opener_1p_frame_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const GURL opener_3p_frame_url =
      embedded_test_server()->GetURL("b.test", "/iframe_blank.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, opener_1p_frame_url));
  RenderFrameHost* parent_frame = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_3p_frame_url));
  if (is_nested_iframe_) {
    parent_frame = ChildFrameAt(parent_frame, 0);
    ASSERT_TRUE(NavigateIframeTo(parent_frame, opener_1p_frame_url));
  }

  PopupObserver observer(web_contents,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_TRUE(
      content::ExecJs(ChildFrameAt(parent_frame, 0),
                      content::JsReplace("window.open($1);", popup_url)));
  observer.Wait();

  auto* popup_tab_helper =
      OpenerHeuristicTabHelper::FromWebContents(observer.popup());
  ASSERT_TRUE(popup_tab_helper);

  if (iframe_types_flag_ == EnableForIframeTypes::kAll) {
    ASSERT_TRUE(popup_tab_helper->popup_observer_for_testing());
  } else {
    ASSERT_FALSE(popup_tab_helper->popup_observer_for_testing());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OpenerHeuristicIframeInitiatorBrowserTest,
    ::testing::Combine(::testing::Values(EnableForIframeTypes::kNone,
                                         EnableForIframeTypes::kFirstParty,
                                         EnableForIframeTypes::kAll),
                       ::testing::Bool()));

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsNotReportedWithoutInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  // Note: no previous interaction on a.test.

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction");
  ASSERT_EQ(entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReported_WithoutRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Since the user landed on the page the popup was opened to, the UKM event
  // has source type NAVIGATION_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicPastInteractionGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<AccessGrantTestCase> {
 public:
  OpenerHeuristicPastInteractionGrantBrowserTest() {
    tpcd_heuristics_grants_params_
        ["TpcdWritePopupPastInteractionHeuristicsGrants"] =
            GetParam().write_grant_enabled ? "20m" : "0s";
    tpcd_heuristics_grants_params_
        ["TpcdPopupHeuristicDisableForAdTaggedPopups"] =
            GetParam().disable_for_ad_tagged_popups ? "true" : "false";
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_P(OpenerHeuristicPastInteractionGrantBrowserTest,
                       PopupPastInteractionIsReported_WithStorageAccessGrant) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  RecordInteraction(initial_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url));
  ASSERT_EQ(popup->GetLastCommittedURL(), final_url);

  // Expect that cookie access was granted for the Popup With Past Interaction
  // heuristic, if the feature is enabled.
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(initial_url, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      GetParam().write_grant_enabled ? CONTENT_SETTING_ALLOW
                                     : CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      cookie_settings->GetThirdPartyCookieAllowMechanism(
          initial_url, opener_url, net::CookieSettingOverrides(), nullptr),
      GetParam().write_grant_enabled
          ? content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCDHeuristics
          : content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kNone);

  // Cookie access was NOT granted for the site that the popup redirected to.
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                final_url, opener_url, net::CookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_P(
    OpenerHeuristicPastInteractionGrantBrowserTest,
    AdTaggedPopupPastInteractionIsReported_WithStorageAccessGrant) {
  GURL opener_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  RecordInteraction(popup_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_TRUE(OpenAdTaggedPopup(popup_url).has_value());

  // Expect that cookie access was granted for the ad-tagged Popup With Past
  // Interaction heuristic, only if the flag is *off*.
  bool should_cookies_be_blocked = !GetParam().write_grant_enabled ||
                                   GetParam().disable_for_ad_tagged_popups;
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                popup_url, opener_url, net::CookieSettingOverrides(), nullptr),
            should_cookies_be_blocked ? CONTENT_SETTING_BLOCK
                                      : CONTENT_SETTING_ALLOW);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicPastInteractionGrantBrowserTest,
                         ::testing::ValuesIn(kAccessGrantTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/1457925): Test is flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  DISABLED_PopupPastInteractionIsReported_ServerRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ServerRedirect \
  PopupPastInteractionIsReported_ServerRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ServerRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url =
      embedded_test_server()->GetURL("a.test", "/server-redirect?title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // Server redirect causes the UKM event to have source type REDIRECT_ID.
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::REDIRECT_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

// TODO(crbug.com/1485029): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  DISABLED_PopupPastInteractionIsReported_ClientRedirect
#else
#define MAYBE_PopupPastInteractionIsReported_ClientRedirect \
  PopupPastInteractionIsReported_ClientRedirect
#endif
IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       MAYBE_PopupPastInteractionIsReported_ClientRedirect) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url =
      embedded_test_server()->GetURL("a.test", "/client-redirect?title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupPastInteraction",
                              {"HoursSinceLastInteraction"});
  ASSERT_EQ(entries.size(), 1u);
  // With a client redirect, we still get a source of type NAVIGATION_ID (since
  // the URL committed).
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  EXPECT_THAT(entries[0].metrics,
              ElementsAre(Pair("HoursSinceLastInteraction", 3)));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsReportedOnlyOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");

  RecordInteraction(GURL("https://a.test"), clock_.Now() - base::Hours(3));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);

  ASSERT_TRUE(content::NavigateToURL(
      *maybe_popup, embedded_test_server()->GetURL("b.test", "/title1.html")));

  // After another navigation, PopupPastInteraction isn't reported again (i.e.,
  // still once total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupPastInteractionIsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  // Initialize interaction and popup.
  RecordInteraction(popup_url, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  int64_t access_id;
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupPastInteraction")
          .size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);
  access_id = top_level_entries[0].metrics["AccessId"];

  base::OnceCallback<void(absl::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](absl::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(access_id, static_cast<int64_t>(state->access_id));
      });
  GetDipsService()
      ->storage()
      ->AsyncCall(&DIPSStorage::ReadPopup)
      .WithArgs(GetSiteForDIPS(opener_url), GetSiteForDIPS(popup_url))
      .Then(std::move(assert_popup));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server.GetURL("sub.b.test", "/favicon/icon.png"));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, PopupInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL redirect_url =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL final_url = embedded_test_server()->GetURL("b.test", "/title1.html");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, redirect_url, final_url));

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      0u);

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction",
                              {"SecondsSinceCommitted", "UrlIndex"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            final_url);
  // The time between *popup_url* committing and the click.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicTimeDelta(
                base::Minutes(2), base::Minutes(3),
                base::BindRepeating(&base::TimeDelta::InSeconds)));
  // The user clicked on *final_url*, which was the third URL.
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 3);
}

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicCurrentInteractionGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<AccessGrantTestCase> {
 public:
  OpenerHeuristicCurrentInteractionGrantBrowserTest() {
    tpcd_heuristics_grants_params_
        ["TpcdWritePopupCurrentInteractionHeuristicsGrants"] =
            GetParam().write_grant_enabled ? "20m" : "0s";
    tpcd_heuristics_grants_params_
        ["TpcdPopupHeuristicDisableForAdTaggedPopups"] =
            GetParam().disable_for_ad_tagged_popups ? "true" : "false";
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_P(OpenerHeuristicCurrentInteractionGrantBrowserTest,
                       PopupInteractionWithStorageAccessGrant) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url));
  ASSERT_EQ(popup->GetLastCommittedURL(), final_url);
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);

  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
  // Cookie access was NOT granted for the initial URL (that the user didn't
  // interact with).
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(initial_url, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  // Cookie access WAS granted for the interacted-with URL (if the feature was
  // enabled).
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                final_url, opener_url, net::CookieSettingOverrides(), nullptr),
            GetParam().write_grant_enabled ? CONTENT_SETTING_ALLOW
                                           : CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_P(OpenerHeuristicCurrentInteractionGrantBrowserTest,
                       AdTaggedPopupInteractionWithStorageAccessGrant) {
  GURL opener_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  auto maybe_popup = OpenAdTaggedPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  // Expect that cookie access was granted for the ad-tagged Popup With Current
  // Interaction heuristic, only if the flag is *off*.
  bool should_cookies_be_blocked = !GetParam().write_grant_enabled ||
                                   GetParam().disable_for_ad_tagged_popups;
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
  EXPECT_EQ(cookie_settings->GetCookieSetting(
                popup_url, opener_url, net::CookieSettingOverrides(), nullptr),
            should_cookies_be_blocked ? CONTENT_SETTING_BLOCK
                                      : CONTENT_SETTING_ALLOW);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicCurrentInteractionGrantBrowserTest,
                         ::testing::ValuesIn(kAccessGrantTestCases));
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteractionIsOnlyReportedOnce) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL interaction_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, interaction_url));
  SimulateMouseClick(*maybe_popup);

  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);

  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, final_url));
  SimulateMouseClick(*maybe_popup);

  // The second click was not reported (still only 1 total).
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PopupInteraction_IgnoreUncommitted) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL popup_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL uncommitted_url = embedded_test_server()->GetURL("c.test", "/nocontent");

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  // Attempt a navigation which won't commit (because the HTTP response is No
  // Content).
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, uncommitted_url, popup_url));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction",
                              {"SecondsSinceCommitted", "UrlIndex"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            popup_url);
  // The uncommitted navigation was ignored. UrlIndex is still 1.
  EXPECT_EQ(entries[0].metrics["SecondsSinceCommitted"],
            Bucketize3PCDHeuristicTimeDelta(
                base::Minutes(2), base::Minutes(3),
                base::BindRepeating(&base::TimeDelta::InSeconds)));
  EXPECT_EQ(entries[0].metrics["UrlIndex"], 1);
}

#if BUILDFLAG(IS_MAC)
// Very flaky on macOS 11 Tests: https://crbug.com/1486448
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  DISABLED_PopupInteraction_IsFollowedByPostPopupCookieAccess
#else
#define MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess \
  PopupInteraction_IsFollowedByPostPopupCookieAccess
#endif
IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    MAYBE_PopupInteraction_IsFollowedByPostPopupCookieAccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url_1 = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL popup_url_2 =
      embedded_test_server()->GetURL("b.test", "/server-redirect?title1.html");
  GURL popup_url_3 = embedded_test_server()->GetURL("b.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  auto maybe_popup = OpenPopup(popup_url_1);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  clock_.Advance(base::Minutes(1));
  ASSERT_TRUE(content::NavigateToURL(*maybe_popup, popup_url_2, popup_url_3));

  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM events and DIPS entries were recorded.
  int64_t access_id;
  ASSERT_EQ(
      ukm_recorder.GetEntriesByName("OpenerHeuristic.PopupInteraction").size(),
      1u);
  auto top_level_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"AccessId"});
  ASSERT_EQ(top_level_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(top_level_entries[0].source_id)->url(),
      opener_url);
  access_id = top_level_entries[0].metrics["AccessId"];

  base::OnceCallback<void(absl::optional<PopupsStateValue>)> assert_popup =
      base::BindLambdaForTesting([&](absl::optional<PopupsStateValue> state) {
        ASSERT_TRUE(state.has_value());
        EXPECT_EQ(access_id, static_cast<int64_t>(state->access_id));
      });
  GetDipsService()
      ->storage()
      ->AsyncCall(&DIPSStorage::ReadPopup)
      .WithArgs(GetSiteForDIPS(opener_url), GetSiteForDIPS(popup_url_3))
      .Then(std::move(assert_popup));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // We host the "image" on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Add a cookie access by popup_url on opener_url.
  ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(), &https_server,
                                  "sub.b.test",
                                  /*is_secure_cookie_set=*/true));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  CreateImageAndWaitForCookieAccess(
      GetActiveWebContents(),
      https_server.GetURL("sub.b.test", "/favicon/icon.png"));
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Assert that the UKM event for the PostPopupCookieAccess was recorded.
  auto access_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PostPopupCookieAccess",
      {"AccessId", "AccessSucceeded", "HoursSincePopupOpened"});
  ASSERT_EQ(access_entries.size(), 1u);
  EXPECT_EQ(
      ukm_recorder.GetSourceForSourceId(access_entries[0].source_id)->url(),
      opener_url);
  EXPECT_EQ(access_entries[0].metrics["AccessId"], access_id);
  EXPECT_EQ(access_entries[0].metrics["AccessSucceeded"], true);
  EXPECT_EQ(access_entries[0].metrics["HoursSincePopupOpened"], 0);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_PastInteraction_NoSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://b.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe", "IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], false);

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupPastInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_NewInteraction_NoSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe", "IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kFalse));
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], false);

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kFalse);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevelIsReported_HasSameSiteIframe) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  GURL iframe_url =
      embedded_test_server()->GetURL("sub.b.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  const std::string iframe_id = "test";
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://b.test"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(content::NavigateIframeToURL(GetActiveWebContents(), iframe_id,
                                           iframe_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kTrue));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupPastInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kTrue);
}

IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    TopLevelIsReported_UnknownSameSiteIframe_OpenerWasClosed) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  DestroyWebContents(web_contents);

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kUnknown));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kUnknown);
}

IN_PROC_BROWSER_TEST_F(
    OpenerHeuristicBrowserTest,
    TopLevelIsNotReported_UnknownSameSiteIframe_OpenerNavigatedAway) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL other_url =
      embedded_test_server()->GetURL("a.test", "/title1.html?other");
  GURL popup_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));

  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  ASSERT_TRUE(content::NavigateToURL(web_contents, other_url));

  ASSERT_EQ(ukm_recorder.GetEntriesByName("OpenerHeuristic.TopLevel").size(),
            0u);

  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"HasSameSiteIframe"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["HasSameSiteIframe"],
            static_cast<int32_t>(OptionalBool::kUnknown));

  auto opener_has_iframe = GetOpenerHasSameSiteIframe(
      ukm_recorder, "OpenerHeuristic.PopupInteraction");
  ASSERT_TRUE(opener_has_iframe.has_value()) << opener_has_iframe.error();
  EXPECT_EQ(opener_has_iframe.value(), OptionalBool::kUnknown);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupProvider) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://google.com"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  ASSERT_TRUE(OpenPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupProvider"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["PopupProvider"],
            static_cast<int64_t>(PopupProvider::kGoogle));
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest, TopLevel_PopupId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url = embedded_test_server()->GetURL("google.com", "/title1.html");
  WebContents* web_contents = GetActiveWebContents();

  RecordInteraction(GURL("https://google.com"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(web_contents, toplevel_url));
  auto maybe_popup = OpenPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();

  SimulateMouseClick(*maybe_popup);

  // Verify all three events share the same popup id.
  auto tl_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(tl_entries[0].source_id)->url(),
            toplevel_url);
  const int64_t popup_id = tl_entries[0].metrics["PopupId"];
  EXPECT_NE(popup_id, 0);

  auto pi_entries =
      ukm_recorder.GetEntries("OpenerHeuristic.PopupInteraction", {"PopupId"});
  ASSERT_EQ(pi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(pi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(pi_entries[0].metrics["PopupId"], popup_id);

  auto ppi_entries = ukm_recorder.GetEntries(
      "OpenerHeuristic.PopupPastInteraction", {"PopupId"});
  ASSERT_EQ(ppi_entries.size(), 1u);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(ppi_entries[0].source_id)->url(),
            popup_url);
  EXPECT_EQ(ppi_entries[0].metrics["PopupId"], popup_id);

  // Open second popup, verify different popup id.
  ASSERT_TRUE(OpenPopup(popup_url).has_value());
  tl_entries = ukm_recorder.GetEntries("OpenerHeuristic.TopLevel", {"PopupId"});
  ASSERT_EQ(tl_entries.size(), 2u);
  const int64_t popup_id2 = tl_entries[1].metrics["PopupId"];
  EXPECT_NE(popup_id2, 0);
  EXPECT_NE(popup_id, popup_id2);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevel_PastInteraction_AdTagged) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  RecordInteraction(GURL("https://b.com"), clock_.Now() - base::Hours(3));

  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), toplevel_url));
  ASSERT_TRUE(OpenAdTaggedPopup(popup_url).has_value());

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], true);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       TopLevel_CurrentInteraction_AdTagged) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL toplevel_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  GURL popup_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), toplevel_url));

  auto maybe_popup = OpenAdTaggedPopup(popup_url);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();
  SimulateMouseClick(*maybe_popup);

  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmEntry> entries =
      ukm_recorder.GetEntries("OpenerHeuristic.TopLevel",
                              {"IsAdTaggedPopupClick"});
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(ukm::GetSourceIdType(entries[0].source_id),
            ukm::SourceIdType::NAVIGATION_ID);
  EXPECT_EQ(ukm_recorder.GetSourceForSourceId(entries[0].source_id)->url(),
            toplevel_url);
  EXPECT_EQ(entries[0].metrics["IsAdTaggedPopupClick"], true);
}

IN_PROC_BROWSER_TEST_F(OpenerHeuristicBrowserTest,
                       PastAndCurrentInteractionAreBothReportedToDipsDb) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL initial_site = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL initial_url = embedded_test_server()->GetURL(
      "b.test", "/cross-site/c.test/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");

  // Initialize popup and interaction.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), initial_site));
  SimulateMouseClick(GetActiveWebContents());

  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_OK_AND_ASSIGN(WebContents * popup, OpenPopup(initial_url));
  ASSERT_EQ(popup->GetLastCommittedURL(), final_url);
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(popup);
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  absl::optional<PopupsStateValue> initial_state =
      GetPopupState(opener_url, initial_url);
  absl::optional<PopupsStateValue> final_state =
      GetPopupState(opener_url, final_url);
  ASSERT_THAT(
      initial_state,
      Optional(Field(&PopupsStateValue::is_current_interaction, false)));
  ASSERT_THAT(final_state,
              Optional(Field(&PopupsStateValue::is_current_interaction, true)));
}

// chrome/browser/ui/browser.h (for changing profile prefs) is not available on
// Android.
#if !BUILDFLAG(IS_ANDROID)
class OpenerHeuristicBackfillGrantBrowserTest
    : public OpenerHeuristicBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  OpenerHeuristicBackfillGrantBrowserTest() {
    tpcd_heuristics_grants_params_["TpcdBackfillPopupHeuristicsGrants"] =
        GetParam() ? "10m" : "0s";
  }

  void SetUpOnMainThread() override {
    OpenerHeuristicBrowserTest::SetUpOnMainThread();

    clock_.SetNow(base::Time::Now());

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }
};

// Test the backfill grants created by DIPSService when tracking protection is
// onboarded. This logic is located in DIPSService in order to run as a
// singleton, rather than once per tab.
IN_PROC_BROWSER_TEST_P(OpenerHeuristicBackfillGrantBrowserTest,
                       TrackingProtectionOnboardingCreatesBackfillGrants) {
  GURL opener_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL popup_url_1 = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL popup_url_2 = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL popup_url_3 = embedded_test_server()->GetURL("d.test", "/title1.html");

  // popup_url_1 was opened further back than the backfill lookback period of 10
  // minutes.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  auto maybe_popup = OpenPopup(popup_url_1);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  clock_.Advance(base::Minutes(10));

  // popup_url_2 was opened with a past interaction, not a current interaction.
  RecordInteraction(popup_url_2, clock_.Now() - base::Hours(3));
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  ASSERT_TRUE(OpenPopup(popup_url_2).has_value());

  // Only popup_url_3 is eligible for a backfill grant.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), opener_url));
  maybe_popup = OpenPopup(popup_url_3);
  ASSERT_TRUE(maybe_popup.has_value()) << maybe_popup.error();
  clock_.Advance(base::Minutes(1));
  SimulateMouseClick(*maybe_popup);

  // The pref is updated when the user in onboarded to 3PCD tracking protection,
  // and a PrefChangeRegistrar updates the TrackingProtectionSettingsObservers.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kTrackingProtection3pcdEnabled, true);
  GetDipsService()->storage()->FlushPostedTasksForTesting();

  // Expect that a cookie access grant is not backfilled for popup_url_1 or
  // popup_url_2.
  auto cookie_settings = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(GetActiveWebContents()->GetBrowserContext()));
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(popup_url_1, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(popup_url_2, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      CONTENT_SETTING_BLOCK);

  // Expect that a cookie access grant is backfilled for popup_url_3 when the
  // experiment is enabled.
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(popup_url_3, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      GetParam() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);

  // Expect that the cookie access grant applies to other URLs with the same
  // eTLD+1.
  GURL popup_url_3a =
      embedded_test_server()->GetURL("www.d.test", "/favicon.png");
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(popup_url_3a, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      GetParam() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  GURL popup_url_3b =
      embedded_test_server()->GetURL("corp.d.test", "/title1.html");
  EXPECT_EQ(
      cookie_settings->GetCookieSetting(popup_url_3b, opener_url,
                                        net::CookieSettingOverrides(), nullptr),
      GetParam() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenerHeuristicBackfillGrantBrowserTest,
                         ::testing::Values(false, true));
#endif  // !BUILDFLAG(IS_ANDROID)
