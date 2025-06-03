// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace webapps {

using State = AppBannerManager::State;

// Browser tests for web app banners.
// NOTE: this test relies on service workers; failures and flakiness may be due
// to changes in SW code.
class AppBannerManagerTest : public AppBannerManager {
 public:
  explicit AppBannerManagerTest(content::WebContents* web_contents)
      : AppBannerManager(web_contents) {}

  AppBannerManagerTest(const AppBannerManagerTest&) = delete;
  AppBannerManagerTest& operator=(const AppBannerManagerTest&) = delete;

  ~AppBannerManagerTest() override {}

  bool TriggeringDisabledForTesting() const override { return false; }

  void RequestAppBanner(const GURL& validated_url) override {
    // Filter out about:blank navigations - we use these in testing to force
    // Stop() to be called.
    if (validated_url == GURL("about:blank"))
      return;

    AppBannerManager::RequestAppBanner(validated_url);
  }

  bool banner_shown() { return banner_shown_.get() && *banner_shown_; }

  WebappInstallSource install_source() {
    if (install_source_.get())
      return *install_source_;

    return WebappInstallSource::COUNT;
  }

  InstallableParams ParamsToPerformInstallableWebAppCheck() override {
    InstallableParams params =
        AppBannerManager::ParamsToPerformInstallableWebAppCheck();
    params.fetch_metadata = true;
    params.installable_criteria =
        base::FeatureList::IsEnabled(features::kUniversalInstallManifest)
            ? InstallableCriteria::kImplicitManifestFieldsHTML
            : InstallableCriteria::kValidManifestWithIcons;

    return params;
  }

  void clear_will_show() { banner_shown_.reset(); }

  State state() { return AppBannerManager::state(); }

  // Configures a callback to be invoked when the app banner flow finishes.
  void PrepareDone(base::OnceClosure on_done) { on_done_ = std::move(on_done); }

  // Configures a callback to be invoked from OnBannerPromptReply.
  void PrepareBannerPromptReply(base::OnceClosure on_banner_prompt_reply) {
    on_banner_prompt_reply_ = std::move(on_banner_prompt_reply);
  }

  bool IsAppFullyInstalledForSiteUrl(const GURL& site_url) const override {
    return false;
  }

  bool IsAppPartiallyInstalledForSiteUrl(const GURL& site_url) const override {
    return false;
  }

  bool IsInAppBrowsingContext() const override { return false; }

  void SaveInstallationDismissedForMl(const GURL& manifest_id) override {}
  void SaveInstallationIgnoredForMl(const GURL& manifest_id) override {}
  void SaveInstallationAcceptedForMl(const GURL& manifest_id) override {}
  bool IsMlPromotionBlockedByHistoryGuardrail(
      const GURL& manifest_id) override {
    return false;
  }
  void OnMlInstallPrediction(base::PassKey<MLInstallabilityPromoter>,
                             std::string result_label) override {}

  segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService() override {
    return nullptr;
  }

 protected:
  // The overridden RequestAppBanner() can filter out about:blank calls
  // to force Stop() to be called, however, the newly introduced
  // AppBannerManagerBrowserTestWithChromeBFCache starts a server and navigates
  // to a dynamic/installable banner link and then retriggers the pipeline by
  // terminating an existing banner. As a result, there can exist banners in an
  // intermediary state (on_done_ not initialized, banner still shown) that
  // needs to be cleaned in these overridden functions for Stop() and
  // UpdateState(State::PENDING).
  //
  // As a result, calls to RequestAppBanner should always terminate in
  // ShowBannerUi(), but not necessarily in one of Stop() (not showing banner)
  // or UpdateState(State::PENDING_ENGAGEMENT) (waiting for sufficient
  // engagement). Override these methods to capture test status.
  void Stop(InstallableStatusCode code) override {
    AppBannerManager::Stop(code);
    if (banner_shown_)
      clear_will_show();
    ASSERT_FALSE(banner_shown_.get());
    banner_shown_ = std::make_unique<bool>(false);
    install_source_ =
        std::make_unique<WebappInstallSource>(WebappInstallSource::COUNT);
    if (on_done_)
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_done_));
  }

  void ShowBannerUi(WebappInstallSource install_source) override {
    // Fake the call to ReportStatus here - this is usually called in
    // platform-specific code which is not exposed here.
    ReportStatus(SHOWING_WEB_APP_BANNER);
    RecordDidShowBanner();

    ASSERT_FALSE(banner_shown_.get());
    banner_shown_ = std::make_unique<bool>(true);
    install_source_ = std::make_unique<WebappInstallSource>(install_source);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_done_));
  }

  void UpdateState(AppBannerManager::State state) override {
    AppBannerManager::UpdateState(state);
    if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
        state == AppBannerManager::State::PENDING_PROMPT_CANCELED ||
        state == AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED) {
      if (on_done_)
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(on_done_));
    }
  }

  void OnBannerPromptReply(
      mojo::Remote<blink::mojom::AppBannerController> controller,
      blink::mojom::AppBannerPromptReply reply) override {
    AppBannerManager::OnBannerPromptReply(std::move(controller), reply);
    if (on_banner_prompt_reply_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_banner_prompt_reply_));
    }
  }

  base::WeakPtr<AppBannerManager> GetWeakPtrForThisNavigation() override {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrsForThisNavigation() override {
    weak_factory_.InvalidateWeakPtrs();
  }

  bool IsSupportedNonWebAppPlatform(
      const std::u16string& platform) const override {
    return base::EqualsASCII(platform, "chrome_web_store");
  }

  bool IsRelatedNonWebAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const override {
    // Corresponds to the id listed in manifest_listing_related_chrome_app.json.
    return base::EqualsASCII(related_app.platform.value_or(std::u16string()),
                             "chrome_web_store") &&
           base::EqualsASCII(related_app.id.value_or(std::u16string()),
                             "installed-extension-id");
  }

  bool IsWebAppConsideredInstalled() const override { return false; }

  base::OnceClosure on_done_;

 private:
  // If non-null, |on_banner_prompt_reply_| will be invoked from
  // OnBannerPromptReply.
  base::OnceClosure on_banner_prompt_reply_;

  std::unique_ptr<bool> banner_shown_;
  std::unique_ptr<WebappInstallSource> install_source_;

  base::WeakPtrFactory<AppBannerManagerTest> weak_factory_{this};
};

class AppBannerManagerBrowserTest : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerBrowserTest()
      : disable_banner_trigger_(&test::g_disable_banner_triggering_for_testing,
                                true),
        total_engagement_(
            AppBannerSettingsHelper::ScopeTotalEngagementForTesting(10)) {}

  AppBannerManagerBrowserTest(const AppBannerManagerBrowserTest&) = delete;
  AppBannerManagerBrowserTest& operator=(const AppBannerManagerBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    site_engagement::SiteEngagementScore::SetParamValuesForTesting();

    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  std::unique_ptr<AppBannerManagerTest> CreateAppBannerManager(
      Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<AppBannerManagerTest>(web_contents);
  }

  void RunBannerTest(
      Browser* browser,
      AppBannerManagerTest* manager,
      const GURL& url,
      absl::optional<InstallableStatusCode> expected_code_for_histogram) {
    base::HistogramTester histograms;

    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(browser->profile());
    service->ResetBaseScoreForURL(url, 10);

    // Spin the run loop and wait for the manager to finish.
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    NavigateParams nav_params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&nav_params);
    run_loop.Run();

    EXPECT_EQ(expected_code_for_histogram.value_or(MAX_ERROR_CODE) ==
                  SHOWING_WEB_APP_BANNER,
              manager->banner_shown());
    EXPECT_EQ(WebappInstallSource::COUNT, manager->install_source());

    // Generally the manager will be in the complete state, however some test
    // cases navigate the page, causing the state to go back to INACTIVE.
    EXPECT_TRUE(manager->state() == State::COMPLETE ||
                manager->state() == State::PENDING_PROMPT_CANCELED ||
                manager->state() == State::PENDING_PROMPT_NOT_CANCELED ||
                manager->state() == State::INACTIVE);

    // If in incognito, ensure that nothing is recorded.
    if (browser->profile()->IsOffTheRecord() || !expected_code_for_histogram) {
      histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
    } else {
      histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                    *expected_code_for_histogram, 1);
    }
  }

  void TriggerBannerFlowWithNavigation(Browser* browser,
                                       AppBannerManagerTest* manager,
                                       const GURL& url,
                                       bool expected_will_show,
                                       absl::optional<State> expected_state) {
    // Use NavigateToURLWithDisposition as it isn't overloaded, so can be used
    // with Bind.
    TriggerBannerFlow(
        browser, manager,
        base::BindOnce(
            base::IgnoreResult(&ui_test_utils::NavigateToURLWithDisposition),
            browser, url, WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP),
        expected_will_show, expected_state);
  }

  void TriggerBannerFlow(Browser* browser,
                         AppBannerManagerTest* manager,
                         base::OnceClosure trigger_task,
                         bool expected_will_show,
                         absl::optional<State> expected_state) {
    base::RunLoop run_loop;
    manager->clear_will_show();
    manager->PrepareDone(run_loop.QuitClosure());
    std::move(trigger_task).Run();
    run_loop.Run();

    EXPECT_EQ(expected_will_show, manager->banner_shown());
    if (expected_state)
      EXPECT_EQ(expected_state, manager->state());
  }

 private:
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<bool> disable_banner_trigger_;
  base::AutoReset<double> total_engagement_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type.json"),
                absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_no_type_caps.json"),
                absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerSvgIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_svg_icon.json"),
                absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerWebPIcon) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_webp_icon.json"),
                absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       DelayedManifestTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      NO_MANIFEST);

  // Dynamically add the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(browser(), manager.get(), base::BindLambdaForTesting([&]() {
                      EXPECT_TRUE(content::ExecJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "addManifestLinkTag()"));
                    }),
                    false,
                    AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       RemovingManifestStopsPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      absl::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  // Dynamically remove the manifest.
  base::HistogramTester histograms;
  TriggerBannerFlow(browser(), manager.get(), base::BindLambdaForTesting([&]() {
                      EXPECT_TRUE(content::ExecJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "removeAllManifestTags()"));
                    }),
                    false, AppBannerManager::State::COMPLETE);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 1);
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ManifestChangeTriggersPipeline) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  // Cause the manifest test page to reach the PENDING_PROMPT stage of the
  // app banner pipeline.
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      absl::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  // Dynamically change the manifest, which results in a
  // Stop(RENDERER_CANCELLED), and a restart of the pipeline.
  {
    base::HistogramTester histograms;
    // Note - The state of the appbannermanager here will be racy, so don't
    // check for that.
    TriggerBannerFlow(
        browser(), manager.get(), base::BindLambdaForTesting([&]() {
          EXPECT_TRUE(content::ExecJs(
              browser()->tab_strip_model()->GetActiveWebContents(),
              "addManifestLinkTag('/banners/manifest_one_icon.json')"));
        }),
        false, absl::nullopt);
    histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 1);
    histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                  RENDERER_CANCELLED, 1);
  }
  // The pipeline should either have completed, or it is scheduled in the
  // background. Wait for the next prompt request if so.
  if (manager->state() !=
      AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED) {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    run_loop.Run();
    histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);
  }
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"),
      NO_MANIFEST);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, MissingManifest) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(browser(), manager.get(),
                GetBannerURLWithManifest("/banners/manifest_missing.json"),
                MANIFEST_EMPTY);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/iframe_test_page.html"),
      NO_MANIFEST);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, DoesNotShowInIncognito) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(incognito_browser));
  RunBannerTest(incognito_browser, manager.get(), GetBannerURL(), IN_INCOGNITO);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerInsufficientEngagement) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  base::HistogramTester histograms;
  GURL test_url = GetBannerURL();

  // First run through: expect the manager to end up stopped in the pending
  // state, without showing a banner.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                INSUFFICIENT_ENGAGEMENT, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerNotCreated) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURL();
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Navigate and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerCancelled) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());

  // Explicitly call preventDefault(), but don't call prompt().
  GURL test_url = GetBannerURLWithAction("cancel_prompt");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt() to be
  // called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_CANCELED);

  // Navigate to about:blank and expect Stop() to be called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerPromptWithGesture) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Now let the page call prompt with a gesture. The banner should be shown.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNeedsEngagement) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::AutoReset<double> scoped_engagement =
      AppBannerSettingsHelper::ScopeTotalEngagementForTesting(1);
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 0);

  // Navigate and expect the manager to end up waiting for sufficient
  // engagement.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);

  // Trigger an engagement increase that signals observers and expect the
  // manager to end up waiting for prompt to be called.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&site_engagement::SiteEngagementService::HandleNavigation,
                     base::Unretained(service),
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     ui::PageTransition::PAGE_TRANSITION_TYPED),
      false /* expected_will_show */, State::PENDING_PROMPT_NOT_CANCELED);

  // Trigger prompt() and expect the banner to be shown.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerReprompt) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());
  GURL test_url = GetBannerURLWithAction("stash_event");
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate to page and get the pipeline started.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Call prompt to show the banner.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  // Dismiss the banner.
  base::RunLoop run_loop;
  manager->PrepareDone(base::DoNothing());
  manager->PrepareBannerPromptReply(run_loop.QuitClosure());
  manager->SendBannerDismissed();
  // Wait for OnBannerPromptReply event.
  run_loop.Run();

  // Call prompt again to show the banner again.
  TriggerBannerFlow(
      browser(), manager.get(),
      base::BindOnce(&AppBannerManagerBrowserTest::ExecuteScript, browser(),
                     "callStashedPrompt();", true /* with_gesture */),
      true /* expected_will_show */, State::COMPLETE);

  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                SHOWING_WEB_APP_BANNER, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PreferRelatedAppUnknown) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_apps_unknown.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_ENGAGEMENT);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PreferRelatedChromeApp) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_prefer_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::COMPLETE);
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                PREFER_RELATED_APPLICATIONS, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       ListedRelatedChromeAppInstalled) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_listing_related_chrome_app.json");
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::COMPLETE);
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                PREFER_RELATED_APPLICATIONS, 1);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerTerminated) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  base::HistogramTester histograms;

  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser()->profile());

  GURL test_url = GetBannerURL();
  service->ResetBaseScoreForURL(test_url, 10);

  // Navigate and expect the manager to end up waiting for prompt() to be
  // called.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), test_url,
                                  false /* expected_will_show */,
                                  State::PENDING_PROMPT_NOT_CANCELED);

  // Navigate to about:blank and expect it to be terminated because the previous
  // URL is still pending.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GURL("about:blank"),
                                  false /* expected_will_show */,
                                  State::INACTIVE);

  // Expect the manifest to be reset to an empty manifest.
  EXPECT_EQ(manager->manifest(), *blink::mojom::Manifest::New());

  // Expect RENDERER_CANCELLED to be called when an existing call is terminated.
  histograms.ExpectUniqueSample(kInstallableStatusCodeHistogram,
                                RENDERER_CANCELLED, 1);
}

class AppBannerManagerBrowserTestWithChromeBFCache
    : public AppBannerManagerBrowserTest {
 public:
  AppBannerManagerBrowserTestWithChromeBFCache() = default;
  ~AppBannerManagerBrowserTestWithChromeBFCache() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For using an HTTPS server.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);

    SetupFeaturesAndParameters();
  }

  void SetupFeaturesAndParameters() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* current_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  GURL Get2ndInstallableURL() {
    return embedded_test_server()->GetURL("/banners/nested_sw_test_page.html");
  }

  bool IsRenderHostStoredInBackForwardCache(content::RenderFrameHost* rfh) {
    return rfh->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  }

  void AssertBackForwardCacheIsUsedAsExpected(
      const content::RenderFrameHostWrapper& rfh) {
    ASSERT_EQ(IsRenderHostStoredInBackForwardCache(rfh.get()),
              content::BackForwardCache::IsBackForwardCacheFeatureEnabled());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTestWithChromeBFCache,
                       VerifyBFCacheBehavior) {
  base::AutoReset<double> scoped_engagement =
      AppBannerSettingsHelper::ScopeTotalEngagementForTesting(1);

  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  base::HistogramTester histograms;
  // Triggering flow to first URL with a pending prompt.
  TriggerBannerFlowWithNavigation(browser(), manager.get(), GetBannerURL(),
                                  /*expected_will_show=*/false,
                                  State::PENDING_PROMPT_NOT_CANCELED);
  content::RenderFrameHostWrapper rfh_a(current_frame_host());
  ASSERT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  histograms.ExpectTotalCount(kInstallableStatusCodeHistogram, 0);

  // Navigating to 2nd installable URL while PENDING_PROMPT will trigger
  // the pipeline.
  TriggerBannerFlowWithNavigation(browser(), manager.get(),
                                  Get2ndInstallableURL(),
                                  /*expected_will_show=*/false, absl::nullopt);
  AssertBackForwardCacheIsUsedAsExpected(rfh_a);

  content::RenderFrameHostWrapper rfh_b(current_frame_host());

  // Navigate backward to 1st installable URL.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  // Verify pipeline has been triggered for new page load.
  EXPECT_NE(manager->state(), AppBannerManager::State::INACTIVE);

  AssertBackForwardCacheIsUsedAsExpected(rfh_b);

  // Navigate forward to 2nd installable URL.
  web_contents()->GetController().GoForward();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  // Verify pipeline has been triggered for new page load.
  EXPECT_NE(manager->state(), AppBannerManager::State::INACTIVE);

  AssertBackForwardCacheIsUsedAsExpected(rfh_a);
}

namespace {
class FailingInstallableManager : public InstallableManager {
 public:
  explicit FailingInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}

  void FailNext(std::unique_ptr<InstallableData> installable_data) {
    failure_data_ = std::move(installable_data);
  }

  void GetData(const InstallableParams& params,
               InstallableCallback callback) override {
    if (failure_data_) {
      auto temp_data = std::move(failure_data_);
      std::move(callback).Run(*temp_data);
      return;
    }
    InstallableManager::GetData(params, std::move(callback));
  }

 private:
  std::unique_ptr<InstallableData> failure_data_;
};

class AppBannerManagerMPArchBrowserTest : public AppBannerManagerBrowserTest {
 public:
  AppBannerManagerMPArchBrowserTest() = default;
  ~AppBannerManagerMPArchBrowserTest() override = default;
  AppBannerManagerMPArchBrowserTest(const AppBannerManagerMPArchBrowserTest&) =
      delete;

  AppBannerManagerMPArchBrowserTest& operator=(
      const AppBannerManagerMPArchBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    AppBannerManagerBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class AppBannerManagerPrerenderBrowserTest
    : public AppBannerManagerMPArchBrowserTest {
 public:
  AppBannerManagerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &AppBannerManagerPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~AppBannerManagerPrerenderBrowserTest() override = default;
  AppBannerManagerPrerenderBrowserTest(
      const AppBannerManagerPrerenderBrowserTest&) = delete;

  AppBannerManagerPrerenderBrowserTest& operator=(
      const AppBannerManagerPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    AppBannerManagerMPArchBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerPrerenderBrowserTest,
                       PrerenderingShouldNotUpdateState) {
  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Load a page in the prerender.
  GURL prerender_url = GetBannerURL();
  const int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_EQ(manager->state(), AppBannerManager::State::FETCHING_MANIFEST);
}

class AppBannerManagerFencedFrameBrowserTest
    : public AppBannerManagerMPArchBrowserTest {
 public:
  AppBannerManagerFencedFrameBrowserTest() = default;
  ~AppBannerManagerFencedFrameBrowserTest() override = default;
  AppBannerManagerFencedFrameBrowserTest(
      const AppBannerManagerFencedFrameBrowserTest&) = delete;

  AppBannerManagerFencedFrameBrowserTest& operator=(
      const AppBannerManagerFencedFrameBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerFencedFrameBrowserTest,
                       FencedFrameShouldNotUpdateState) {
  // Navigate to an initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Initialize a MockWebContentsObserver to ensure that DidUpdateManifestURL is
  // not invoked for fenced frame.
  testing::NiceMock<content::MockWebContentsObserver> observer(
      GetWebContents());

  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Create a fenced frame.
  GURL fenced_frame_url = embedded_test_server()->GetURL(
      "/banners/fenced_frames/manifest_test_page.html?manifest=/banners/"
      "manifest.json");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);

  // Cross check that  DidUpdateWebManifestURL is not called for fenced frame
  // RenderFrameHost.
  EXPECT_CALL(observer, DidUpdateWebManifestURL(fenced_frame_host, testing::_))
      .Times(0);

  // Navigate the fenced frame.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            fenced_frame_url);
  EXPECT_EQ(manager->state(), AppBannerManager::State::INACTIVE);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, ShowBanner) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      absl::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
            AppBannerManager::InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoServiceWorker) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  RunBannerTest(browser(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/manifest_no_service_worker.html"),
                /*expected_code_for_histogram=*/absl::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
            AppBannerManager::InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoFetchHandler) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  RunBannerTest(browser(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/no_sw_fetch_handler_test_page.html"),
                /*expected_code_for_histogram=*/absl::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
            AppBannerManager::InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PendingServiceWorker) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<AppBannerManagerTest> manager =
      std::make_unique<AppBannerManagerTest>(web_contents);

  RunBannerTest(browser(), manager.get(),
                embedded_test_server()->GetURL(
                    "/banners/manifest_no_service_worker.html"),
                absl::nullopt);

  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);

  EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
            AppBannerManager::InstallableWebAppCheckResult::kYes_Promotable);

  EXPECT_EQ(manager->GetAppName(), u"Manifest test app");
}

enum class InstallableCriteriaType {
  kValidManifestWithIcons,
  kImplicitManifestFields
};

class AppBannerInstallCriteriaTest
    : public AppBannerManagerBrowserTest,
      public testing::WithParamInterface<InstallableCriteriaType> {
 public:
  AppBannerInstallCriteriaTest() {
    switch (GetParam()) {
      case InstallableCriteriaType::kValidManifestWithIcons:
        scoped_feature_list_.InitWithFeatures(
            {}, {features::kUniversalInstallManifest,
                 features::kUniversalInstallIcon});
        break;
      case InstallableCriteriaType::kImplicitManifestFields:
        scoped_feature_list_.InitWithFeatures(
            {features::kUniversalInstallManifest,
             features::kUniversalInstallIcon},
            {});
        break;
    }
  }

 public:
  ~AppBannerInstallCriteriaTest() override = default;

  AppBannerInstallCriteriaTest(const AppBannerInstallCriteriaTest&) = delete;
  AppBannerInstallCriteriaTest& operator=(const AppBannerInstallCriteriaTest&) =
      delete;

  void SetUpOnMainThread() override {
    AppBannerManagerBrowserTest::SetUpOnMainThread();
  }

  void CheckBannerResult(AppBannerManagerTest* manager) {
    ASSERT_EQ(manager->state(), AppBannerManager::State::COMPLETE);
    if (GetParam() == InstallableCriteriaType::kValidManifestWithIcons) {
      EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
                AppBannerManager::InstallableWebAppCheckResult::kNo);
    } else {  // InstallableCriteriaType::kImplicitManifestFields
      EXPECT_EQ(
          manager->GetInstallableWebAppCheckResultForTesting(),
          AppBannerManager::InstallableWebAppCheckResult::kYes_ByUserRequest);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppBannerInstallCriteriaTest, ValidManifestShowBanner) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));
  RunBannerTest(
      browser(), manager.get(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
      absl::nullopt);
  EXPECT_EQ(manager->state(),
            AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED);
  EXPECT_EQ(manager->GetInstallableWebAppCheckResultForTesting(),
            AppBannerManager::InstallableWebAppCheckResult::kYes_Promotable);
}

IN_PROC_BROWSER_TEST_P(AppBannerInstallCriteriaTest, kImplicitName) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_empty_name_short_name.json&application-name=TestApp");

  RunBannerTest(browser(), manager.get(), test_url,
                MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  CheckBannerResult(manager.get());
  if (GetParam() == InstallableCriteriaType::kImplicitManifestFields) {
    EXPECT_EQ(manager->GetAppName(), u"TestApp");
  }
}

IN_PROC_BROWSER_TEST_P(AppBannerInstallCriteriaTest,
                       kImplicitNameDocumentTitle) {
  std::unique_ptr<AppBannerManagerTest> manager(
      CreateAppBannerManager(browser()));

  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest="
      "manifest_empty_name_short_name.json");

  RunBannerTest(browser(), manager.get(), test_url,
                MANIFEST_MISSING_NAME_OR_SHORT_NAME);

  CheckBannerResult(manager.get());
  if (GetParam() == InstallableCriteriaType::kImplicitManifestFields) {
    EXPECT_EQ(manager->GetAppName(), u"Web app banner test page");
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AppBannerInstallCriteriaTest,
    testing::Values(InstallableCriteriaType::kValidManifestWithIcons,
                    InstallableCriteriaType::kImplicitManifestFields));

}  // namespace
}  // namespace webapps
