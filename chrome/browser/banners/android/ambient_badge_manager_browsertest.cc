// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/android/ambient_badge_manager.h"
#include "components/webapps/browser/android/app_banner_manager_android.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace webapps {

class TestAmbientBadgeManager : public AmbientBadgeManager {
 public:
  explicit TestAmbientBadgeManager(
      content::WebContents* web_contents,
      base::WeakPtr<AppBannerManagerAndroid> app_banner_manager)
      : AmbientBadgeManager(web_contents, app_banner_manager, nullptr) {}

  TestAmbientBadgeManager(const TestAmbientBadgeManager&) = delete;
  TestAmbientBadgeManager& operator=(const TestAmbientBadgeManager&) = delete;

  ~TestAmbientBadgeManager() override = default;

  void WaitForState(State target, base::OnceClosure on_done) {
    target_state_ = target;
    on_done_ = std::move(on_done);
  }

 protected:
  void UpdateState(State state) override {
    AmbientBadgeManager::UpdateState(state);
    if (state == target_state_ && on_done_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_done_));
    }
  }

 private:
  State target_state_;
  base::OnceClosure on_done_;
};

class TestAppBannerManager : public AppBannerManagerAndroid {
 public:
  explicit TestAppBannerManager(content::WebContents* web_contents)
      : AppBannerManagerAndroid(web_contents) {}

  TestAppBannerManager(const TestAppBannerManager&) = delete;
  TestAppBannerManager& operator=(const TestAppBannerManager&) = delete;

  ~TestAppBannerManager() override = default;

  void WaitForAmbientBadgeState(AmbientBadgeManager::State target,
                                base::OnceClosure on_done) {
    target_badge_state_ = target;
    on_badge_done_ = std::move(on_done);
  }

  bool TriggeringDisabledForTesting() const override { return false; }

  TestAmbientBadgeManager* GetBadgeManagerForTest() {
    return ambient_badge_test_.get();
  }

 protected:
  void MaybeShowAmbientBadge() override {
    ambient_badge_test_ = std::make_unique<TestAmbientBadgeManager>(
        web_contents(), GetAndroidWeakPtr());

    ambient_badge_test_->WaitForState(target_badge_state_,
                                      std::move(on_badge_done_));
    ambient_badge_test_->MaybeShow(
        validated_url_, GetAppName(),
        CreateAddToHomescreenParams(InstallableMetrics::GetInstallSource(
            web_contents(), InstallTrigger::AMBIENT_BADGE)),
        base::BindOnce(&AppBannerManagerAndroid::ShowBannerFromBadge,
                       AppBannerManagerAndroid::GetAndroidWeakPtr()));
  }

 private:
  std::unique_ptr<TestAmbientBadgeManager> ambient_badge_test_;
  AmbientBadgeManager::State target_badge_state_;
  base::OnceClosure on_badge_done_;
};

class AmbientBadgeManagerBrowserTest : public AndroidBrowserTest {
 public:
  AmbientBadgeManagerBrowserTest()
      : disable_banner_trigger_(&test::g_disable_banner_triggering_for_testing,
                                true) {}

  AmbientBadgeManagerBrowserTest(const AmbientBadgeManagerBrowserTest&) =
      delete;
  AmbientBadgeManagerBrowserTest& operator=(
      const AmbientBadgeManagerBrowserTest&) = delete;

  ~AmbientBadgeManagerBrowserTest() override = default;

  void SetUp() override {
    SetUpFeatureList();
    AndroidBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    site_engagement::SiteEngagementScore::SetParamValuesForTesting();

    AndroidBrowserTest::SetUpOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

  virtual void SetUpFeatureList() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAmbientBadgeSuppressFirstVisit);
  }

  void ResetEngagementForUrl(const GURL& url, double score) {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    service->ResetBaseScoreForURL(url, score);
  }

  void RunTest(TestAppBannerManager* app_banner_manager,
               const GURL& url,
               AmbientBadgeManager::State expected_state) {
    ResetEngagementForUrl(url, 10);

    base::RunLoop waiter;

    app_banner_manager->WaitForAmbientBadgeState(expected_state,
                                                 waiter.QuitClosure());
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

    waiter.Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<bool> disable_banner_trigger_;
};

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerBrowserTest, ShowAmbientBadge) {
  auto app_banner_manager =
      std::make_unique<TestAppBannerManager>(web_contents());

  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kShowing);
}

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerBrowserTest, NoServiceWorker) {
  auto app_banner_manager =
      std::make_unique<TestAppBannerManager>(web_contents());
  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL(
              "/banners/manifest_no_service_worker.html"),
          AmbientBadgeManager::State::kPendingWorker);
}

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerBrowserTest,
                       BlockedIfDismissRecently) {
  auto app_banner_manager =
      std::make_unique<TestAppBannerManager>(web_contents());

  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kShowing);

  // Explicitly dismiss the badge.
  app_banner_manager->GetBadgeManagerForTest()->BadgeDismissed();  // IN-TEST
  EXPECT_EQ(AmbientBadgeManager::State::kDismissed,
            app_banner_manager->GetBadgeManagerForTest()->state());  // IN-TEST

  // Badge blocked because it was recently dismissed.
  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kBlocked);

  // Badge can show again after 91 days.
  webapps::AppBannerManager::SetTimeDeltaForTesting(91);
  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kShowing);
}

class AmbientBadgeManagerSecondVisitTest
    : public AmbientBadgeManagerBrowserTest {
 public:
  AmbientBadgeManagerSecondVisitTest() = default;

  AmbientBadgeManagerSecondVisitTest(
      const AmbientBadgeManagerSecondVisitTest&) = delete;
  AmbientBadgeManagerSecondVisitTest& operator=(
      const AmbientBadgeManagerSecondVisitTest&) = delete;

  ~AmbientBadgeManagerSecondVisitTest() override = default;

  void SetUpFeatureList() override {}
};

IN_PROC_BROWSER_TEST_F(AmbientBadgeManagerSecondVisitTest,
                       SuppressedOnFirstVisit) {
  auto app_banner_manager =
      std::make_unique<TestAppBannerManager>(web_contents());

  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kPendingEngagement);

  // Load again, can show.
  RunTest(app_banner_manager.get(),
          embedded_test_server()->GetURL("/banners/manifest_test_page.html"),
          AmbientBadgeManager::State::kShowing);
}

}  // namespace webapps
