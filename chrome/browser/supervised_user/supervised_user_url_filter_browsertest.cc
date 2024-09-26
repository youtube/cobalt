// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

// Tests filtering for supervised users.
class SupervisedUserURLFilterTest : public MixinBasedInProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  SupervisedUserURLFilterTest() {
    // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }
  ~SupervisedUserURLFilterTest() override = default;

  bool ShownPageIsInterstitial(Browser* browser) {
    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(tab->IsCrashed());
    std::u16string title;
    ui_test_utils::GetCurrentTabTitle(browser, &title);
    return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
               content::PAGE_TYPE_ERROR &&
           title == u"Site blocked";
  }

  void SendAccessRequest(WebContents* tab) {
    tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"supervisedUserErrorPageController.requestPermission()",
        base::NullCallback());
    return;
  }

  void GoBack(WebContents* tab) {
    tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"supervisedUserErrorPageController.goBack()", base::NullCallback());
    return;
  }

  void GoBackAndWaitForNavigation(WebContents* tab) {
    content::TestNavigationObserver observer(tab);
    GoBack(tab);
    observer.Wait();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Remap all URLs to test server.
    ASSERT_TRUE(embedded_test_server()->Started());
    std::string host_port = embedded_test_server()->host_port_pair().ToString();
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    "MAP *.example.com " + host_port + "," +
                                        "MAP *.new-example.com " + host_port +
                                        "," + "MAP *.a.com " + host_port);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();

    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  }

  // Acts like a synchronous call to history's QueryHistory. Modified from
  // history_querying_unittest.cc.
  void QueryHistory(history::HistoryService* history_service,
                    const std::string& text_query,
                    const history::QueryOptions& options,
                    history::QueryResults* results) {
    base::RunLoop run_loop;
    base::CancelableTaskTracker history_task_tracker;
    history_service->QueryHistory(
        base::UTF8ToUTF16(text_query), options,
        base::BindLambdaForTesting([&](history::QueryResults r) {
          *results = std::move(r);
          run_loop.Quit();
        }),
        &history_task_tracker);
    run_loop.Run();  // Will go until ...Complete calls Quit.
  }

  base::test::ScopedFeatureList feature_list_;
  raw_ptr<SupervisedUserService, ExperimentalAsh> supervised_user_service_ =
      nullptr;

  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, ash::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
};

// Tests the filter mode in which all sites are blocked by default.
class SupervisedUserBlockModeTest : public SupervisedUserURLFilterTest {
 public:
  void SetUpOnMainThread() override {
    SupervisedUserURLFilterTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service =
            SupervisedUserSettingsServiceFactory::GetForKey(
                profile->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackDefaultFilteringBehavior,
        base::Value(supervised_user::SupervisedUserURLFilter::BLOCK));
  }
};

class TabClosingObserver : public TabStripModelObserver {
 public:
  TabClosingObserver(TabStripModel* tab_strip, content::WebContents* contents)
      : tab_strip_(tab_strip), contents_(contents) {
    tab_strip_->AddObserver(this);
  }

  TabClosingObserver(const TabClosingObserver&) = delete;
  TabClosingObserver& operator=(const TabClosingObserver&) = delete;

  void WaitForContentsClosing() {
    if (!contents_)
      return;

    run_loop_.Run();
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kRemoved)
      return;

    auto* remove = change.GetRemove();
    for (const auto& contents : remove->contents) {
      if (contents_ == contents.contents &&
          contents.remove_reason ==
              TabStripModelChange::RemoveReason::kDeleted) {
        if (run_loop_.running())
          run_loop_.Quit();
        contents_ = nullptr;
        return;
      }
    }
  }

 private:
  raw_ptr<TabStripModel, ExperimentalAsh> tab_strip_ = nullptr;

  base::RunLoop run_loop_;

  // Contents to wait for.
  raw_ptr<content::WebContents, ExperimentalAsh> contents_ = nullptr;
};

// Navigates to a blocked URL.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       SendAccessRequestOnBlockedURL) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  SendAccessRequest(tab);

  // TODO(sergiu): Properly check that the access request was sent here.

  GoBackAndWaitForNavigation(tab);

  // Make sure that the tab is still there.
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(ShownPageIsInterstitial(browser()));
}

// Navigates to a blocked URL in a new tab. We expect the tab to be closed
// automatically on pressing the "back" button on the interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, OpenBlockedURLInNewTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();

  // Open blocked URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that we got the interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // On pressing the "back" button, the new tab should be closed, and we should
  // get back to the previous active tab.
  TabClosingObserver observer(tab_strip, tab);
  GoBack(tab);
  observer.WaitForContentsClosing();

  EXPECT_EQ(prev_tab, tab_strip->GetActiveWebContents());
}

// Navigates to a page in a new tab, then blocks it (which makes the
// interstitial page behave differently from the preceding test, where the
// navigation is blocked before it commits). The expected behavior is the same
// though: the tab should be closed when going back.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockNewTabAfterLoading) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();

  // Open URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  {
    // Block the current URL.
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service =
            SupervisedUserSettingsServiceFactory::GetForKey(
                browser()->profile()->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackDefaultFilteringBehavior,
        base::Value(supervised_user::SupervisedUserURLFilter::BLOCK));

    supervised_user::SupervisedUserURLFilter* filter =
        supervised_user_service_->GetURLFilter();
    ASSERT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
              filter->GetFilteringBehaviorForURL(test_url));

    content::TestNavigationObserver observer(tab);
    observer.Wait();

    // Check that we got the interstitial.
    ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  }

  {
    // On pressing the "back" button, the new tab should be closed, and we
    // should get back to the previous active tab.
    TabClosingObserver observer(tab_strip, tab);
    GoBack(tab);
    observer.WaitForContentsClosing();
    EXPECT_EQ(prev_tab, tab_strip->GetActiveWebContents());
  }
}

// Tests that we don't end up canceling an interstitial (thereby closing the
// whole tab) by attempting to show a second one above it.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, DontShowInterstitialTwice) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Open URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Block the current URL.
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(supervised_user::SupervisedUserURLFilter::BLOCK));

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver observer(tab);
  observer.Wait();

  // Check that we got the interstitial.
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // Trigger a no-op change to the site lists, which will notify observers of
  // the URL filter.
  supervised_user_service_->OnSiteListUpdated();

  EXPECT_EQ(tab, tab_strip->GetActiveWebContents());
}

// Tests that it's possible to navigate from a blocked page to another blocked
// page.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       NavigateFromBlockedPageToBlockedPage) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  GURL test_url2("http://www.a.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url2));

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  EXPECT_EQ(test_url2, tab->GetVisibleURL());
}

// Tests whether a visit attempt adds a special history entry.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, HistoryVisitRecorded) {
  GURL allowed_url("http://www.example.com/simple.html");

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();

  // Set the host as allowed.
  base::Value::Dict dict;
  dict.Set(allowed_url.host(), true);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
  EXPECT_EQ(supervised_user::SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url));
  EXPECT_EQ(supervised_user::SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));

  // Navigate to it and check that we don't get an interstitial.
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Navigate to a blocked page and go back on the interstitial.
  GURL blocked_url("http://www.new-example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GoBackAndWaitForNavigation(tab);

  EXPECT_EQ(allowed_url.spec(), tab->GetLastCommittedURL().spec());
  EXPECT_EQ(supervised_user::SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));
  EXPECT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(blocked_url.GetWithEmptyPath()));

  // Query the history entry.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions options;
  history::QueryResults results;
  QueryHistory(history_service, "", options, &results);

  // Check that the entries have the correct blocked_visit value.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(blocked_url.spec(), results[1].url().spec());
  EXPECT_TRUE(results[1].blocked_visit());
  EXPECT_EQ(allowed_url.spec(), results[0].url().spec());
  EXPECT_FALSE(results[0].blocked_visit());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, GoBackOnDontProceed) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure navigation completes.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  // We start out at the initial navigation.
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  base::Value::Dict dict;
  dict.Set(test_url.host(), false);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  content::LoadStopObserver observer(web_contents);
  GoBack(web_contents);
  observer.Wait();

  // We should have gone back to the initial navigation.
  EXPECT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest,
                       ClosingBlockedTabDoesNotCrash) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure navigation completes.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  base::Value::Dict dict;
  dict.Set(test_url.host(), false);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  // Verify that there is no crash when closing the blocked tab
  // (https://crbug.com/719708).
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockThenUnblock) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  // Set the host as blocked and wait for the interstitial to appear.
  {
    base::Value::Dict dict;
    dict.Set(test_url.host(), false);
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
  }

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(supervised_user::SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  {
    base::Value::Dict dict;
    dict.Set(test_url.host(), true);
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
    ASSERT_EQ(supervised_user::SupervisedUserURLFilter::ALLOW,
              filter->GetFilteringBehaviorForURL(test_url));
  }

  content::TestNavigationObserver unblock_observer(web_contents);
  unblock_observer.Wait();

  ASSERT_EQ(test_url, web_contents->GetLastCommittedURL());

  EXPECT_FALSE(ShownPageIsInterstitial(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, Unblock) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  content::LoadStopObserver observer(web_contents);

  // Set the host as allowed.
  base::Value::Dict dict;
  dict.Set(test_url.host(), true);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  EXPECT_EQ(supervised_user::SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(test_url.GetWithEmptyPath()));

  observer.Wait();
  EXPECT_EQ(test_url, web_contents->GetLastCommittedURL());
}

class MockSupervisedUserURLFilterObserver
    : public supervised_user::SupervisedUserURLFilter::Observer {
 public:
  explicit MockSupervisedUserURLFilterObserver(
      supervised_user::SupervisedUserURLFilter* filter)
      : filter_(filter) {
    filter_->AddObserver(this);
  }
  ~MockSupervisedUserURLFilterObserver() { filter_->RemoveObserver(this); }

  MockSupervisedUserURLFilterObserver(
      const MockSupervisedUserURLFilterObserver&) = delete;
  MockSupervisedUserURLFilterObserver& operator=(
      const MockSupervisedUserURLFilterObserver&) = delete;

  // SupervisedUserURLFilter::Observer:
  void OnSiteListUpdated() override {}
  MOCK_METHOD(
      void,
      OnURLChecked,
      (const GURL& url,
       supervised_user::SupervisedUserURLFilter::FilteringBehavior behavior,
       supervised_user::FilteringBehaviorReason reason,
       bool uncertain),
      (override));

 private:
  const raw_ptr<supervised_user::SupervisedUserURLFilter, ExperimentalAsh>
      filter_;
};

class SupervisedUserURLFilterPrerenderingTest
    : public SupervisedUserURLFilterTest {
 public:
  SupervisedUserURLFilterPrerenderingTest()
      : prerender_test_helper_(base::BindRepeating(
            &SupervisedUserURLFilterPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}
  ~SupervisedUserURLFilterPrerenderingTest() override = default;

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

// Tests that prerendering doesn't check SupervisedUserURLFilter.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterPrerenderingTest, OnURLChecked) {
  MockSupervisedUserURLFilterObserver observer(
      supervised_user_service_->GetURLFilter());

  GURL test_url("http://www.example.com/simple.html");
  EXPECT_CALL(observer, OnURLChecked).Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Load a page in prerendering.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());
  // We do not yet support prerendering for supervised users and prerendering is
  // canceled even though it tries to start prerendering. So, OnURLChecked() is
  // never called in prerendering.
  EXPECT_CALL(observer, OnURLChecked).Times(0);
  GURL prerender_url("http://www.example.com/title1.html");
  // Try prerendering.
  prerender_helper().AddPrerenderAsync(prerender_url);
  // Ensure that prerendering has started.
  registry_observer.WaitForTrigger(prerender_url);
  auto prerender_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_NE(content::RenderFrameHost::kNoFrameTreeNodeId, prerender_id);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     prerender_id);
  // Prerendering is canceled.
  host_observer.WaitForDestroyed();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Navigate the primary page to the URL.
  EXPECT_CALL(observer, OnURLChecked).Times(1);
  prerender_helper().NavigatePrimaryPage(prerender_url);
}

}  // namespace
