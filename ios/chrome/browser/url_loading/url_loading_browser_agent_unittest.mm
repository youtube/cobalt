// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/url_loading/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/test_scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface URLLoadingTestDelegate : NSObject <URLLoadingDelegate>
@end

@implementation URLLoadingTestDelegate

#pragma mark - URLLoadingBrowserAgentDelegate

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
}

@end

#pragma mark -

namespace {
class URLLoadingBrowserAgentTest : public BlockCleanupTest {
 public:
  URLLoadingBrowserAgentTest() {
    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
    otr_browser_state_ =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    url_loading_delegate_ = [[URLLoadingTestDelegate alloc] init];
    scene_loader_ = std::make_unique<TestSceneUrlLoadingService>();
    otr_browser_ = std::make_unique<TestBrowser>(otr_browser_state_);
    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    // Configure app service.
    scene_loader_->current_browser_ = browser_.get();
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    SceneStateBrowserAgent::CreateForBrowser(otr_browser_.get(), scene_state_);

    // Disable web usage on both browsers
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    WebUsageEnablerBrowserAgent* enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler->SetWebUsageEnabled(false);
    WebUsageEnablerBrowserAgent::CreateForBrowser(otr_browser_.get());
    WebUsageEnablerBrowserAgent* otr_enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(otr_browser_.get());
    otr_enabler->SetWebUsageEnabled(false);

    // Create loaders, insertion and notifier agents.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    loader_ = UrlLoadingBrowserAgent::FromBrowser(browser_.get());
    loader_->SetDelegate(url_loading_delegate_);
    loader_->SetSceneService(scene_loader_.get());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(otr_browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(otr_browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(otr_browser_.get());
    otr_loader_ = UrlLoadingBrowserAgent::FromBrowser(otr_browser_.get());
    otr_loader_->SetDelegate(url_loading_delegate_);
    otr_loader_->SetSceneService(scene_loader_.get());

    loader_->SetIncognitoLoader(otr_loader_);
  }

  void TearDown() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = browser_->GetWebStateList();
    web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
    otr_web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);

    BlockCleanupTest::TearDown();
  }

  // Returns a new unique_ptr containing a test webstate.
  std::unique_ptr<web::FakeWebState> CreateFakeWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(chrome_browser_state_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    return web_state;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ChromeBrowserState* otr_browser_state_;
  URLLoadingTestDelegate* url_loading_delegate_;
  std::unique_ptr<TestSceneUrlLoadingService> scene_loader_;
  UrlLoadingBrowserAgent* loader_;
  std::unique_ptr<Browser> otr_browser_;
  UrlLoadingBrowserAgent* otr_loader_;
  SceneState* scene_state_;
};

TEST_F(URLLoadingBrowserAgentTest, TestSwitchToTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("http://test/1"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::FakeWebState> web_state_2 = CreateFakeWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests that switch to open tab from the NTP close it if it doesn't have
// navigation history.
TEST_F(URLLoadingBrowserAgentTest, TestSwitchToTabFromNTP) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  id mock_delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state_ptr);
  NewTabPageTabHelper::FromWebState(web_state_ptr)->SetDelegate(mock_delegate);

  std::unique_ptr<web::FakeWebState> web_state_2 = CreateFakeWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests that trying to switch to a closed tab open from the NTP opens it in the
// NTP.
TEST_F(URLLoadingBrowserAgentTest, TestSwitchToClosedTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list->ActivateWebStateAt(0);
  id mock_delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state_ptr);
  NewTabPageTabHelper::FromWebState(web_state_ptr)->SetDelegate(mock_delegate);

  GURL url("http://test/2");

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_ptr, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests open a new url in the NTP or the current tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInCurrentTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab, so we can open in it.
  GURL newtab("chrome://newtab");
  loader_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Test opening this url over NTP.
  GURL url1("http://test/1");
  loader_->Load(
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1)));

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());
  // And that a new tab wasn't created.
  EXPECT_EQ(1, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests opening a url in a new tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInNewTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab.
  GURL newtab("chrome://newtab");
  loader_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Open another one.
  GURL url("http://test/2");
  loader_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(2, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests open a new url in the current incognito tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInCurrentIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  std::unique_ptr<TestBrowser> otr_browser = std::make_unique<TestBrowser>(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  scene_loader_->current_browser_ = otr_browser.get();

  // Set a new tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_loader_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Open otr request with otr service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, otr_web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());

  // And that a new tab wasn't created.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests opening a url in a new incognito tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInNewIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  std::unique_ptr<TestBrowser> otr_browser = std::make_unique<TestBrowser>(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  scene_loader_->current_browser_ = otr_browser.get();

  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = YES;
  otr_loader_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(2, otr_web_state_list->count());

  // Check if we had any app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Test opening a normal url in new tab with incognito service.
TEST_F(URLLoadingBrowserAgentTest, TestOpenNormalInNewTabWithIncognitoService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  std::unique_ptr<TestBrowser> otr_browser = std::make_unique<TestBrowser>(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  scene_loader_->current_browser_ = otr_browser.get();

  // Send to right service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to wrong service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  otr_loader_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that had one app level redirection.
  EXPECT_EQ(1, scene_loader_->load_new_tab_call_count_);
}

// Test opening an incognito url in new tab with normal service.
TEST_F(URLLoadingBrowserAgentTest, TestOpenIncognitoInNewTabWithNormalService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  scene_loader_->current_browser_ = browser_.get();

  // Send to wrong service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Send to right service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  loader_->Load(params2);
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, scene_loader_->load_new_tab_call_count_);
}

// Test opening an incognito url in new tab with normal service using load
// strategy.
TEST_F(URLLoadingBrowserAgentTest, TestOpenIncognitoInNewTabWithLoadStrategy) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  scene_loader_->current_browser_ = browser_.get();

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, scene_loader_->load_new_tab_call_count_);
}

// Test opening an incognito url in current tab with normal service using load
// strategy.
TEST_F(URLLoadingBrowserAgentTest,
       TestOpenIncognitoInCurrentTabWithLoadStrategy) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  std::unique_ptr<TestBrowser> otr_browser = std::make_unique<TestBrowser>(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  scene_loader_->current_browser_ = otr_browser.get();

  // Set a new incognito tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_loader_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  loader_->Load(params1);

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, otr_web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());

  // And that a new tab wasn't created.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

}  // namespace
