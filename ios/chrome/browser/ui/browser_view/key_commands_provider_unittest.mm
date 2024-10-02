// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/java_script_find_tab_helper.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/sessions/fake_tab_restore_service.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/find_in_page/java_script_find_in_page_manager_impl.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<KeyedService> BuildFakeTabRestoreService(
    web::BrowserState* browser_state) {
  return std::make_unique<FakeTabRestoreService>();
}

class KeyCommandsProviderTest : public PlatformTest {
 protected:
  KeyCommandsProviderTest() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(IOSChromeTabRestoreServiceFactory::GetInstance(),
                              base::BindRepeating(BuildFakeTabRestoreService));
    builder.AddTestingFactory(
        ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
        ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    web_state_list_ = browser_->GetWebStateList();
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
    bookmark_model_ =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    provider_ = [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  }
  ~KeyCommandsProviderTest() override {}

  web::FakeWebState* InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    int insertedIndex = web_state_list_->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    return static_cast<web::FakeWebState*>(
        web_state_list_->GetWebStateAt(insertedIndex));
  }

  void CloseWebState(int index) {
    web_state_list_->CloseWebStateAt(
        index, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }

  // Checks that `view_controller_` can perform the `action` with the given
  // `sender`.
  bool CanPerform(NSString* action, id sender) {
    return [provider_ canPerformAction:NSSelectorFromString(action)
                            withSender:sender];
  }

  // Checks that `view_controller_` can perform the `action`. The sender is set
  // to nil when performing this check.
  bool CanPerform(NSString* action) { return CanPerform(action, nil); }

  // Creates a web state with a back list with 2 elements.
  web::FakeWebState* InsertNewWebPageWithMultipleEntries(int index) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();

    std::unique_ptr<web::FakeNavigationManager> navigation_manager =
        std::make_unique<web::FakeNavigationManager>();
    GURL url1("http:/test1.test/");
    navigation_manager->AddItem(url1, ui::PageTransition::PAGE_TRANSITION_LINK);
    GURL url2("http:/test2.test/");
    navigation_manager->AddItem(url2, ui::PageTransition::PAGE_TRANSITION_LINK);
    GURL url3("http:/test3.test/");
    navigation_manager->AddItem(url3, ui::PageTransition::PAGE_TRANSITION_LINK);

    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());

    int insertedIndex = web_state_list_->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_ACTIVATE,
        WebStateOpener());
    return static_cast<web::FakeWebState*>(
        web_state_list_->GetWebStateAt(insertedIndex));
  }

  // Creates a FakeWebState with a navigation history containing exactly only
  // the given `url`.
  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    return web_state;
  }

  void ExpectUMA(NSString* action, const std::string& user_action) {
    ASSERT_EQ(user_action_tester_.GetActionCount(user_action), 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [provider_ performSelector:NSSelectorFromString(action)];
#pragma clang diagnostic pop
    EXPECT_EQ(user_action_tester_.GetActionCount(user_action), 1);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebStateList* web_state_list_;
  SceneState* scene_state_;
  base::UserActionTester user_action_tester_;
  bookmarks::BookmarkModel* bookmark_model_;
  KeyCommandsProvider* provider_;
};

// Checks that KeyCommandsProvider returns key commands.
TEST_F(KeyCommandsProviderTest, ReturnsKeyCommands) {
  EXPECT_NE(0u, provider_.keyCommands.count);
}

#pragma mark - Responder Chain Tests

// Checks that the nextResponder is nil by default.
TEST_F(KeyCommandsProviderTest, NextResponderUnset) {
  EXPECT_EQ(provider_.nextResponder, nil);
}

// Checks that the nextResponder is correctly set.
TEST_F(KeyCommandsProviderTest, NextResponderSet) {
  UIResponder* responder = [[UIResponder alloc] init];

  [provider_ respondBetweenViewController:nil andResponder:responder];

  EXPECT_EQ(provider_.nextResponder, responder);
}

// Checks that nextResponder is reset to nil.
TEST_F(KeyCommandsProviderTest, NextResponderReset) {
  UIResponder* responder = [[UIResponder alloc] init];
  [provider_ respondBetweenViewController:nil andResponder:responder];
  ASSERT_EQ(provider_.nextResponder, responder);

  [provider_ respondBetweenViewController:nil andResponder:nil];

  EXPECT_EQ(provider_.nextResponder, nil);
}

#pragma mark - CanPerform Tests

// Checks whether KeyCommandsProvider can perform the actions that are always
// available.
TEST_F(KeyCommandsProviderTest, CanPerform_AlwaysAvailableActions) {
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewRegularTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewIncognitoTab"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewWindow"));
  EXPECT_TRUE(CanPerform(@"keyCommand_openNewIncognitoWindow"));
  EXPECT_TRUE(CanPerform(@"keyCommand_showSettings"));
  EXPECT_TRUE(CanPerform(@"keyCommand_showReadingList"));
  EXPECT_TRUE(CanPerform(@"keyCommand_goToTabGrid"));
  EXPECT_TRUE(CanPerform(@"keyCommand_clearBrowsingData"));
}

// Checks whether KeyCommandsProvider can perform the actions that are always
// available when there is a presented view controller.
TEST_F(KeyCommandsProviderTest,
       CanPerform_AlwaysAvailableActions_PresentedViewController) {
  UIViewController* viewController = [[UIViewController alloc] init];
  [GetAnyKeyWindow() addSubview:viewController.view];
  [provider_ respondBetweenViewController:viewController andResponder:nil];
  UIViewController* presentedViewController = [[UIViewController alloc] init];
  [viewController presentViewController:presentedViewController
                               animated:NO
                             completion:nil];

  EXPECT_FALSE(CanPerform(@"keyCommand_openNewTab"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewRegularTab"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewIncognitoTab"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewWindow"));
  EXPECT_FALSE(CanPerform(@"keyCommand_openNewIncognitoWindow"));
  EXPECT_FALSE(CanPerform(@"keyCommand_showSettings"));
  EXPECT_FALSE(CanPerform(@"keyCommand_showReadingList"));
  EXPECT_FALSE(CanPerform(@"keyCommand_goToTabGrid"));
  EXPECT_FALSE(CanPerform(@"keyCommand_clearBrowsingData"));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs.
TEST_F(KeyCommandsProviderTest, CanPerform_TabsActions) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  NSArray<NSString*>* actions = @[
    @"keyCommand_openLocation",  @"keyCommand_closeTab",
    @"keyCommand_showBookmarks", @"keyCommand_reload",
    @"keyCommand_showHistory",   @"keyCommand_voiceSearch",
    @"keyCommand_stop",          @"keyCommand_showHelp",
    @"keyCommand_showDownloads", @"keyCommand_select1",
    @"keyCommand_select2",       @"keyCommand_select3",
    @"keyCommand_select4",       @"keyCommand_select5",
    @"keyCommand_select6",       @"keyCommand_select7",
    @"keyCommand_select8",       @"keyCommand_select9",
    @"keyCommand_showNextTab",   @"keyCommand_showPreviousTab",
  ];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a tab.
  InsertNewWebState(0);
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }

  // Close the tab.
  CloseWebState(0);
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and Find in Page is available. Ensure that Find
// Next and Find Previous are not shown.
TEST_F(KeyCommandsProviderTest, CanPerform_FindInPageActions) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Open a tab.
  web::FakeWebState* web_state = InsertNewWebState(0);
  web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                 std::make_unique<web::FakeWebFramesManager>());
  web::JavaScriptFindInPageManagerImpl::CreateForWebState(web_state);
  JavaScriptFindTabHelper::CreateForWebState(web_state);

  // No Find in Page.
  web_state->SetContentIsHTML(false);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));

  // Can Find in Page.
  web_state->SetContentIsHTML(true);
  EXPECT_TRUE(CanPerform(@"keyCommand_find"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Find UI active.
  JavaScriptFindTabHelper* helper =
      JavaScriptFindTabHelper::FromWebState(web_state);
  helper->SetFindUIActive(YES);
  EXPECT_TRUE(CanPerform(@"keyCommand_findNext"));
  EXPECT_TRUE(CanPerform(@"keyCommand_findPrevious"));

  helper->SetFindUIActive(NO);
  EXPECT_FALSE(CanPerform(@"keyCommand_findNext"));
  EXPECT_FALSE(CanPerform(@"keyCommand_findPrevious"));

  // Close the tab.
  CloseWebState(0);
  EXPECT_FALSE(CanPerform(@"keyCommand_find"));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and text is being edited.
TEST_F(KeyCommandsProviderTest, CanPerform_EditingTextActions) {
  // back_2 and forward_2 conflict with text editing commands, so they should be
  // ignored.
  UIKeyCommand* back_2 = UIKeyCommand.cr_back_2;
  UIKeyCommand* forward_2 = UIKeyCommand.cr_forward_2;
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);

  EXPECT_FALSE(CanPerform(@"keyCommand_back"));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));

  // Add one with back and forward list not empty.
  web::FakeWebState* web_state = InsertNewWebPageWithMultipleEntries(0);
  // Ensure you have go back and go forward enabled.
  web_navigation_util::GoBack(web_state);

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_TRUE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward", forward_2));

  // Focus a text field.
  UITextField* textField = [[UITextField alloc] init];
  [GetAnyKeyWindow() addSubview:textField];
  [textField becomeFirstResponder];

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));

  // Reset the first responder.
  [textField resignFirstResponder];

  EXPECT_TRUE(CanPerform(@"keyCommand_back"));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward"));
  EXPECT_TRUE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_TRUE(CanPerform(@"keyCommand_forward", forward_2));

  // Close the tab.
  CloseWebState(0);

  EXPECT_FALSE(CanPerform(@"keyCommand_back"));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward"));
  EXPECT_FALSE(CanPerform(@"keyCommand_back", back_2));
  EXPECT_FALSE(CanPerform(@"keyCommand_forward", forward_2));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are tabs and it is a http or https page.
TEST_F(KeyCommandsProviderTest, CanPerform_ActionsInHttpPage) {
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  NSArray<NSString*>* actions =
      @[ @"keyCommand_addToBookmarks", @"keyCommand_addToReadingList" ];
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a New Tab Page (NTP) tab which is not a http or https page.
  std::unique_ptr<web::FakeNavigationManager> fake_navigation_manager =
      std::make_unique<web::FakeNavigationManager>();

  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  pending_item->SetURL(GURL(kChromeUIAboutNewTabURL));

  fake_navigation_manager->SetPendingItem(pending_item.get());
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>();

  GURL url(kChromeUINewTabURL);
  fake_web_state->SetVisibleURL(url);
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
  fake_web_state->SetBrowserState(browser_state_.get());

  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));

  NewTabPageTabHelper::CreateForWebState(fake_web_state.get());
  NewTabPageTabHelper* ntp_helper =
      NewTabPageTabHelper::FromWebState(fake_web_state.get());
  ntp_helper->SetDelegate(delegate);

  // Ensure that the actions are not available when the tab is a NTP.
  ASSERT_TRUE(ntp_helper->IsActive());
  ASSERT_FALSE(url.SchemeIsHTTPOrHTTPS());
  for (NSString* action in actions) {
    EXPECT_FALSE(CanPerform(action));
  }

  // Open a second tab which is a http one.
  web::FakeWebState* second_tab_web_state = InsertNewWebState(1);
  GURL http_url("http://foo/");
  second_tab_web_state->SetVisibleURL(http_url);

  // Ensure that the actions are available.
  ASSERT_TRUE(http_url.SchemeIsHTTPOrHTTPS());
  for (NSString* action in actions) {
    EXPECT_TRUE(CanPerform(action));
  }
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are back or forward navigations.
TEST_F(KeyCommandsProviderTest, CanPerform_BackForwardWithMultipleEntries) {
  web::FakeWebState* web_state = InsertNewWebPageWithMultipleEntries(0);

  NSString* goBackActions = @"keyCommand_back";
  NSString* goForwardActions = @"keyCommand_forward";

  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_FALSE(CanPerform(goForwardActions));

  web_navigation_util::GoBack(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoBack(web_state);
  EXPECT_FALSE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoForward(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_TRUE(CanPerform(goForwardActions));

  web_navigation_util::GoForward(web_state);
  EXPECT_TRUE(CanPerform(goBackActions));
  EXPECT_FALSE(CanPerform(goForwardActions));
}

// Checks whether KeyCommandsProvider can perform the actions that are only
// available when there are at least one closed tab.
TEST_F(KeyCommandsProviderTest, CanPerform_ReopenLastClosedTab) {
  ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser_.get());
  // No tabs.
  ASSERT_EQ(web_state_list_->count(), 0);
  EXPECT_FALSE(CanPerform(@"keyCommand_reopenLastClosedTab"));

  // Add three new tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURL("https://test/url1"));
  browser_->GetWebStateList()->InsertWebState(0, std::move(web_state1),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  auto web_state2 = CreateFakeWebStateWithURL(GURL("https://test/url2"));
  browser_->GetWebStateList()->InsertWebState(1, std::move(web_state2),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  auto web_state3 = CreateFakeWebStateWithURL(GURL("https://test/url3"));
  browser_->GetWebStateList()->InsertWebState(2, std::move(web_state3),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  browser_->GetWebStateList()->ActivateWebStateAt(0);
  EXPECT_FALSE(CanPerform(@"keyCommand_reopenLastClosedTab"));

  // Close a tab.
  CloseWebState(1);
  EXPECT_TRUE(CanPerform(@"keyCommand_reopenLastClosedTab"));
}

// Checks whether KeyCommandsProvider supports user feedback.
TEST_F(KeyCommandsProviderTest, CanPerform_ReportAnIssue) {
  EXPECT_EQ(CanPerform(@"keyCommand_reportAnIssue"),
            ios::provider::IsUserFeedbackSupported());
}

#pragma mark - Metrics Tests

// Checks that metrics are correctly reported.
TEST_F(KeyCommandsProviderTest, Metrics) {
  ExpectUMA(@"keyCommand_openNewTab", "MobileKeyCommandOpenNewTab");
  ExpectUMA(@"keyCommand_openNewRegularTab",
            "MobileKeyCommandOpenNewRegularTab");
  ExpectUMA(@"keyCommand_openNewIncognitoTab",
            "MobileKeyCommandOpenNewIncognitoTab");
  ExpectUMA(@"keyCommand_openNewWindow", "MobileKeyCommandOpenNewWindow");
  ExpectUMA(@"keyCommand_openNewIncognitoWindow",
            "MobileKeyCommandOpenNewIncognitoWindow");
  ExpectUMA(@"keyCommand_reopenLastClosedTab",
            "MobileKeyCommandReopenLastClosedTab");
  ExpectUMA(@"keyCommand_find", "MobileKeyCommandFind");
  ExpectUMA(@"keyCommand_findNext", "MobileKeyCommandFindNext");
  ExpectUMA(@"keyCommand_findPrevious", "MobileKeyCommandFindPrevious");
  ExpectUMA(@"keyCommand_openLocation", "MobileKeyCommandOpenLocation");
  ExpectUMA(@"keyCommand_closeTab", "MobileKeyCommandCloseTab");
  ExpectUMA(@"keyCommand_showNextTab", "MobileKeyCommandShowNextTab");
  ExpectUMA(@"keyCommand_showPreviousTab", "MobileKeyCommandShowPreviousTab");
  ExpectUMA(@"keyCommand_showBookmarks", "MobileKeyCommandShowBookmarks");
  ExpectUMA(@"keyCommand_addToBookmarks", "MobileKeyCommandAddToBookmarks");
  ExpectUMA(@"keyCommand_reload", "MobileKeyCommandReload");
  ExpectUMA(@"keyCommand_back", "MobileKeyCommandBack");
  ExpectUMA(@"keyCommand_forward", "MobileKeyCommandForward");
  ExpectUMA(@"keyCommand_showHistory", "MobileKeyCommandShowHistory");
  ExpectUMA(@"keyCommand_voiceSearch", "MobileKeyCommandVoiceSearch");
  ExpectUMA(@"keyCommand_showSettings", "MobileKeyCommandShowSettings");
  ExpectUMA(@"keyCommand_stop", "MobileKeyCommandStop");
  ExpectUMA(@"keyCommand_showHelp", "MobileKeyCommandShowHelp");
  ExpectUMA(@"keyCommand_showDownloads", "MobileKeyCommandShowDownloads");
  ExpectUMA(@"keyCommand_select1", "MobileKeyCommandShowFirstTab");
  ExpectUMA(@"keyCommand_select2", "MobileKeyCommandShowTab2");
  ExpectUMA(@"keyCommand_select3", "MobileKeyCommandShowTab3");
  ExpectUMA(@"keyCommand_select4", "MobileKeyCommandShowTab4");
  ExpectUMA(@"keyCommand_select5", "MobileKeyCommandShowTab5");
  ExpectUMA(@"keyCommand_select6", "MobileKeyCommandShowTab6");
  ExpectUMA(@"keyCommand_select7", "MobileKeyCommandShowTab7");
  ExpectUMA(@"keyCommand_select8", "MobileKeyCommandShowTab8");
  ExpectUMA(@"keyCommand_select9", "MobileKeyCommandShowLastTab");
  ExpectUMA(@"keyCommand_reportAnIssue", "MobileKeyCommandReportAnIssue");
  ExpectUMA(@"keyCommand_addToReadingList", "MobileKeyCommandAddToReadingList");
  ExpectUMA(@"keyCommand_showReadingList", "MobileKeyCommandShowReadingList");
  ExpectUMA(@"keyCommand_goToTabGrid", "MobileKeyCommandGoToTabGrid");
  ExpectUMA(@"keyCommand_clearBrowsingData",
            "MobileKeyCommandClearBrowsingData");
}

#pragma mark - Actions Tests

// Checks that KeyCommandsProvider implements the following actions.
TEST_F(KeyCommandsProviderTest, ImplementsActions) {
  [provider_ keyCommand_openNewTab];
  [provider_ keyCommand_openNewRegularTab];
  [provider_ keyCommand_openNewIncognitoTab];
  [provider_ keyCommand_openNewWindow];
  [provider_ keyCommand_openNewIncognitoWindow];
  [provider_ keyCommand_reopenLastClosedTab];
  [provider_ keyCommand_find];
  [provider_ keyCommand_findNext];
  [provider_ keyCommand_findPrevious];
  [provider_ keyCommand_openLocation];
  [provider_ keyCommand_closeTab];
  [provider_ keyCommand_showNextTab];
  [provider_ keyCommand_showPreviousTab];
  [provider_ keyCommand_showBookmarks];
  [provider_ keyCommand_addToBookmarks];
  [provider_ keyCommand_reload];
  [provider_ keyCommand_back];
  [provider_ keyCommand_forward];
  [provider_ keyCommand_showHistory];
  [provider_ keyCommand_voiceSearch];
  [provider_ keyCommand_showSettings];
  [provider_ keyCommand_stop];
  [provider_ keyCommand_showHelp];
  [provider_ keyCommand_showDownloads];
  [provider_ keyCommand_select1];
  [provider_ keyCommand_select2];
  [provider_ keyCommand_select3];
  [provider_ keyCommand_select4];
  [provider_ keyCommand_select5];
  [provider_ keyCommand_select6];
  [provider_ keyCommand_select7];
  [provider_ keyCommand_select8];
  [provider_ keyCommand_select9];
  [provider_ keyCommand_reportAnIssue];
  [provider_ keyCommand_addToReadingList];
  [provider_ keyCommand_showReadingList];
  [provider_ keyCommand_goToTabGrid];
  [provider_ keyCommand_clearBrowsingData];
}

// Checks the openNewTab logic based on a regular browser state.
TEST_F(KeyCommandsProviderTest, OpenNewTab_RegularBrowserState) {
  id handler = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  provider_.dispatcher = handler;
  id newTabCommand = [OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
    return command.shouldFocusOmnibox == YES && command.inIncognito == NO;
  }];
  OCMExpect([provider_.dispatcher openURLInNewTab:newTabCommand]);

  [provider_ keyCommand_openNewTab];
}

// Checks the openNewTab logic based on an incognito browser state.
TEST_F(KeyCommandsProviderTest, OpenNewTab_IncognitoBrowserState) {
  ChromeBrowserState* incognito_browser_state =
      browser_state_->GetOffTheRecordChromeBrowserState();
  browser_ = std::make_unique<TestBrowser>(incognito_browser_state);
  provider_ = [[KeyCommandsProvider alloc] initWithBrowser:browser_.get()];
  id handler = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  provider_.dispatcher = handler;
  id newTabCommand = [OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
    return command.shouldFocusOmnibox == YES && command.inIncognito == YES;
  }];
  OCMExpect([provider_.dispatcher openURLInNewTab:newTabCommand]);

  [provider_ keyCommand_openNewTab];
}

// Checks that openNewRegularTab opens a tab in the regular browser state.
TEST_F(KeyCommandsProviderTest, OpenNewRegularTab) {
  id handler = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  provider_.dispatcher = handler;
  id newTabCommand = [OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
    return command.shouldFocusOmnibox == YES && command.inIncognito == NO;
  }];
  OCMExpect([provider_.dispatcher openURLInNewTab:newTabCommand]);

  [provider_ keyCommand_openNewTab];
}

// Checks that openNewIncognitoTab opens a tab in the Incognito browser state.
TEST_F(KeyCommandsProviderTest, OpenNewIncognitoTab) {
  id handler = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  provider_.dispatcher = handler;
  id newTabCommand = [OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
    return command.shouldFocusOmnibox == YES && command.inIncognito == YES;
  }];
  OCMExpect([provider_.dispatcher openURLInNewTab:newTabCommand]);

  [provider_ keyCommand_openNewIncognitoTab];
}

// Checks the next/previous tab actions work OK.
TEST_F(KeyCommandsProviderTest, NextPreviousTab) {
  InsertNewWebState(0);
  InsertNewWebState(1);
  InsertNewWebState(2);
  ASSERT_EQ(web_state_list_->count(), 3);

  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 1);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 2);
  [provider_ keyCommand_showNextTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 2);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 1);
  [provider_ keyCommand_showPreviousTab];
  EXPECT_EQ(web_state_list_->active_index(), 0);
}

// Verifies that the Bookmarks are asked to be shown.
TEST_F(KeyCommandsProviderTest, ShowBookmarks) {
  id handler = OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  provider_.browserCoordinatorCommandsHandler = handler;
  OCMExpect([provider_.browserCoordinatorCommandsHandler showBookmarksManager]);

  [provider_ keyCommand_showBookmarks];

  [handler verify];
}

// Verifies that nothing is added to Bookmarks when there is no tab.
TEST_F(KeyCommandsProviderTest, AddToBookmarks_DoesntAddWhenNoTab) {
  provider_.bookmarksCommandsHandler =
      OCMStrictProtocolMock(@protocol(BookmarksCommands));

  [provider_ keyCommand_addToBookmarks];
}

// Verifies that nothing is added to Bookmarks when on the NTP.
TEST_F(KeyCommandsProviderTest, AddToBookmarks_DoesntAddWhenNTP) {
  provider_.bookmarksCommandsHandler =
      OCMStrictProtocolMock(@protocol(BookmarksCommands));
  InsertNewWebState(0);

  [provider_ keyCommand_addToBookmarks];
}

// Verifies that the correct URL is added to Bookmarks.
TEST_F(KeyCommandsProviderTest, AddToBookmarks_AddURL) {
  id handler = OCMStrictProtocolMock(@protocol(BookmarksCommands));
  provider_.bookmarksCommandsHandler = handler;
  GURL url = GURL("https://e.test");
  id addCommand = [OCMArg checkWithBlock:^BOOL(BookmarkAddCommand* command) {
    return command.URLs.count == 1 && command.URLs.firstObject.URL == url;
  }];
  OCMExpect([provider_.bookmarksCommandsHandler bookmark:addCommand]);
  web::FakeWebState* web_state = InsertNewWebState(0);
  web_state->SetCurrentURL(url);

  [provider_ keyCommand_addToBookmarks];

  [handler verify];
}

// Verifies that the Reading List is asked to be shown.
TEST_F(KeyCommandsProviderTest, ShowReadingList) {
  id handler = OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  provider_.browserCoordinatorCommandsHandler = handler;
  OCMExpect([provider_.browserCoordinatorCommandsHandler showReadingList]);

  [provider_ keyCommand_showReadingList];

  [handler verify];
}

// Verifies that nothing is added to Reading List when there is no tab.
TEST_F(KeyCommandsProviderTest, AddToReadingList_DoesntAddWhenNoTab) {
  provider_.dispatcher = OCMStrictProtocolMock(@protocol(ApplicationCommands));

  [provider_ keyCommand_addToReadingList];
}

// Verifies that nothing is added to Reading List when on the NTP.
TEST_F(KeyCommandsProviderTest, AddToReadingList_DoesntAddWhenNTP) {
  provider_.dispatcher = OCMStrictProtocolMock(@protocol(ApplicationCommands));
  InsertNewWebState(0);

  [provider_ keyCommand_addToReadingList];
}

// Verifies that showing the tab at a given index is a no-op when there are no
// tabs.
TEST_F(KeyCommandsProviderTest, ShowTabAtIndex_NoTab) {
  ASSERT_EQ(web_state_list_->count(), 0);

  [provider_ keyCommand_select1];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select2];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select3];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select4];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select5];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select6];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select7];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select8];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
  [provider_ keyCommand_select9];
  EXPECT_EQ(web_state_list_->active_index(), WebStateList::kInvalidIndex);
}

// Verifies that showing the tab at a given index is a no-op when there is one
// tab.
TEST_F(KeyCommandsProviderTest, ShowTabAtIndex_OneTab) {
  InsertNewWebState(0);
  ASSERT_EQ(web_state_list_->count(), 1);

  [provider_ keyCommand_select1];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select2];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select3];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select4];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select5];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select6];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select7];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select8];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select9];
  EXPECT_EQ(web_state_list_->active_index(), 0);
}

// Verifies that showing the tab at a given index is showing the correct tab.
TEST_F(KeyCommandsProviderTest, ShowTabAtIndex_SomeTabs) {
  InsertNewWebState(0);
  InsertNewWebState(1);
  InsertNewWebState(2);
  InsertNewWebState(3);
  InsertNewWebState(4);
  InsertNewWebState(5);
  InsertNewWebState(6);
  InsertNewWebState(7);
  InsertNewWebState(8);
  InsertNewWebState(9);
  InsertNewWebState(10);
  ASSERT_EQ(web_state_list_->count(), 11);

  [provider_ keyCommand_select1];
  EXPECT_EQ(web_state_list_->active_index(), 0);
  [provider_ keyCommand_select2];
  EXPECT_EQ(web_state_list_->active_index(), 1);
  [provider_ keyCommand_select3];
  EXPECT_EQ(web_state_list_->active_index(), 2);
  [provider_ keyCommand_select4];
  EXPECT_EQ(web_state_list_->active_index(), 3);
  [provider_ keyCommand_select5];
  EXPECT_EQ(web_state_list_->active_index(), 4);
  [provider_ keyCommand_select6];
  EXPECT_EQ(web_state_list_->active_index(), 5);
  [provider_ keyCommand_select7];
  EXPECT_EQ(web_state_list_->active_index(), 6);
  [provider_ keyCommand_select8];
  EXPECT_EQ(web_state_list_->active_index(), 7);
  [provider_ keyCommand_select9];
  EXPECT_EQ(web_state_list_->active_index(), 10);
}

// Verifies that KeyCommandsProvider performs the correct back/forward
// navigation actions.
TEST_F(KeyCommandsProviderTest, BackForward) {
  web::FakeWebState* web_state = InsertNewWebPageWithMultipleEntries(0);
  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  int initial_index = navigation_manager->GetLastCommittedItemIndex();

  [provider_ keyCommand_back];
  EXPECT_EQ(navigation_manager->GetLastCommittedItemIndex(), initial_index - 1);

  [provider_ keyCommand_back];
  EXPECT_EQ(navigation_manager->GetLastCommittedItemIndex(), initial_index - 2);

  [provider_ keyCommand_forward];
  EXPECT_EQ(navigation_manager->GetLastCommittedItemIndex(), initial_index - 1);

  [provider_ keyCommand_forward];
  EXPECT_EQ(navigation_manager->GetLastCommittedItemIndex(), initial_index);
}

#pragma mark - Validate

TEST_F(KeyCommandsProviderTest, ValidateCommands) {
  // Open a tab.
  web::FakeWebState* web_state = InsertNewWebState(0);
  web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                 std::make_unique<web::FakeWebFramesManager>());
  web::JavaScriptFindInPageManagerImpl::CreateForWebState(web_state);
  JavaScriptFindTabHelper::CreateForWebState(web_state);

  // Can Find in Page.
  web_state->SetContentIsHTML(true);
  EXPECT_TRUE(CanPerform(@"keyCommand_find"));
  EXPECT_TRUE(CanPerform(@"keyCommand_select1"));

  for (UIKeyCommand* command in provider_.keyCommands) {
    [provider_ validateCommand:command];
    if (command.action == @selector(keyCommand_find)) {
      EXPECT_TRUE([command.discoverabilityTitle
          isEqualToString:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_KEYBOARD_FIND_IN_PAGE)]);
    }
    if (command.action == @selector(keyCommand_select1)) {
      EXPECT_TRUE([command.discoverabilityTitle
          isEqualToString:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_KEYBOARD_FIRST_TAB)]);
    }
  }
}

TEST_F(KeyCommandsProviderTest, ValidateBookmarkCommand) {
  // Open a tab with a URL.
  GURL url = GURL("https://test/url");
  auto web_state = CreateFakeWebStateWithURL(url);
  browser_->GetWebStateList()->InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  for (UIKeyCommand* command in provider_.keyCommands) {
    [provider_ validateCommand:command];
    if (command.action == @selector(keyCommand_addToBookmarks)) {
      EXPECT_NSEQ(
          command.discoverabilityTitle,
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS));
    }
  }

  // Bookmark the page.
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model_->AddURL(bookmark_bar, 0, u"", url);

  for (UIKeyCommand* command in provider_.keyCommands) {
    [provider_ validateCommand:command];
    if (command.action == @selector(keyCommand_addToBookmarks)) {
      EXPECT_NSEQ(
          command.discoverabilityTitle,
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_EDIT_BOOKMARK));
    }
  }

  // Remove the bookmark.
  bookmark_model_->Remove(bookmark,
                          bookmarks::metrics::BookmarkEditSource::kOther);

  for (UIKeyCommand* command in provider_.keyCommands) {
    [provider_ validateCommand:command];
    if (command.action == @selector(keyCommand_addToBookmarks)) {
      EXPECT_NSEQ(
          command.discoverabilityTitle,
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS));
    }
  }
}

// Checks that clearing the Browser doesn't lead to a crash.
TEST_F(KeyCommandsProviderTest, ClearingBrowserDoesntCrash) {
  InsertNewWebState(0);
  EXPECT_TRUE(CanPerform(@"keyCommand_showNextTab"));

  browser_.reset();

  EXPECT_FALSE(CanPerform(@"keyCommand_showNextTab"));
}

}  // namespace
