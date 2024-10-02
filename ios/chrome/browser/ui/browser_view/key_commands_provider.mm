// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#import <objc/runtime.h>

#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/sessions/core/tab_restore_service_helper.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/abstract_find_tab_helper.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/public/commands/bookmark_add_command.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;

@interface KeyCommandsProvider () {
  // The current browser object.
  base::WeakPtr<Browser> _browser;
}

// The view controller delegating key command actions handling.
@property(nonatomic, weak) UIViewController* viewController;

// Configures the responder following the receiver in the responder chain.
@property(nonatomic, weak) UIResponder* followingNextResponder;

// The current navigation agent.
@property(nonatomic, assign, readonly)
    WebNavigationBrowserAgent* navigationAgent;

// Whether the Find in Page… UI is currently available.
@property(nonatomic, readonly, getter=isFindInPageAvailable)
    BOOL findInPageAvailable;

// The number of tabs displayed.
@property(nonatomic, readonly) NSUInteger tabsCount;

// Whether text is currently being edited.
@property(nonatomic, readonly, getter=isEditingText) BOOL editingText;

@end

@implementation KeyCommandsProvider

#pragma mark - Public

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (void)respondBetweenViewController:(UIViewController*)viewController
                        andResponder:(UIResponder*)nextResponder {
  _viewController = viewController;
  _followingNextResponder = nextResponder;
}

#pragma mark - UIResponder

- (UIResponder*)nextResponder {
  return _followingNextResponder;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  // On iOS 15+, key commands visible in the app's menu are created in
  // MenuBuilder.
  if (@available(iOS 15, *)) {
    // Return the key commands that are not already present in the menu.
    return @[
      UIKeyCommand.cr_openNewRegularTab,
      UIKeyCommand.cr_showNextTab_2,
      UIKeyCommand.cr_showPreviousTab_2,
      UIKeyCommand.cr_showNextTab_3,
      UIKeyCommand.cr_showPreviousTab_3,
      UIKeyCommand.cr_back_2,
      UIKeyCommand.cr_forward_2,
      UIKeyCommand.cr_showDownloads_2,
      UIKeyCommand.cr_select2,
      UIKeyCommand.cr_select3,
      UIKeyCommand.cr_select4,
      UIKeyCommand.cr_select5,
      UIKeyCommand.cr_select6,
      UIKeyCommand.cr_select7,
      UIKeyCommand.cr_select8,
      UIKeyCommand.cr_reportAnIssue_2,
    ];
  } else {
    // Return all the commands supported by BrowserViewController.
    return @[
      UIKeyCommand.cr_openNewTab,
      UIKeyCommand.cr_openNewIncognitoTab,
      UIKeyCommand.cr_reopenLastClosedTab,
      UIKeyCommand.cr_find,
      UIKeyCommand.cr_findNext,
      UIKeyCommand.cr_findPrevious,
      UIKeyCommand.cr_openLocation,
      UIKeyCommand.cr_closeTab,
      UIKeyCommand.cr_showNextTab,
      UIKeyCommand.cr_showPreviousTab,
      UIKeyCommand.cr_showNextTab_2,
      UIKeyCommand.cr_showPreviousTab_2,
      UIKeyCommand.cr_showNextTab_3,
      UIKeyCommand.cr_showPreviousTab_3,
      UIKeyCommand.cr_showBookmarks,
      UIKeyCommand.cr_addToBookmarks,
      UIKeyCommand.cr_reload,
      UIKeyCommand.cr_back,
      UIKeyCommand.cr_forward,
      UIKeyCommand.cr_back_2,
      UIKeyCommand.cr_forward_2,
      UIKeyCommand.cr_showHistory,
      UIKeyCommand.cr_voiceSearch,
      UIKeyCommand.cr_openNewRegularTab,
      UIKeyCommand.cr_showSettings,
      UIKeyCommand.cr_stop,
      UIKeyCommand.cr_showHelp,
      UIKeyCommand.cr_showDownloads,
      UIKeyCommand.cr_showDownloads_2,
      UIKeyCommand.cr_select1,
      UIKeyCommand.cr_select2,
      UIKeyCommand.cr_select3,
      UIKeyCommand.cr_select4,
      UIKeyCommand.cr_select5,
      UIKeyCommand.cr_select6,
      UIKeyCommand.cr_select7,
      UIKeyCommand.cr_select8,
      UIKeyCommand.cr_select9,
    ];
  }
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // If the browser disappeared, prevent any command handling.
  if (!_browser) {
    return NO;
  }

  // BVC prevents KeyCommandsProvider from providing key commands when it has
  // `presentedViewController` set. But there is an interval between presenting
  // a view controller and having `presentedViewController` set. In that window,
  // KeyCommandsProvider can register key commands while it shouldn't.
  // To prevent actions from executing, check again if there is a
  // `presentedViewController`.
  if (_viewController.presentedViewController) {
    return NO;
  }

  if (sel_isEqual(action, @selector(keyCommand_back))) {
    BOOL canPerformBack =
        self.tabsCount > 0 && self.navigationAgent->CanGoBack();
    // Since cmd+left is a valid system shortcuts when editing text, register it
    // only if text is not being edited.
    if ([sender isEqual:UIKeyCommand.cr_back_2]) {
      return canPerformBack && !self.editingText;
    }
    return canPerformBack;
  }

  if (sel_isEqual(action, @selector(keyCommand_forward))) {
    BOOL canPerformForward =
        self.tabsCount > 0 && self.navigationAgent->CanGoForward();
    // Since cmd+right is a valid system shortcuts when editing text, register
    // it only if text is not being edited.
    if ([sender isEqual:UIKeyCommand.cr_forward_2]) {
      return canPerformForward && !self.editingText;
    }
    return canPerformForward;
  }
  if (sel_isEqual(action, @selector(keyCommand_openLocation)) ||
      sel_isEqual(action, @selector(keyCommand_closeTab)) ||
      sel_isEqual(action, @selector(keyCommand_showBookmarks)) ||
      sel_isEqual(action, @selector(keyCommand_reload)) ||
      sel_isEqual(action, @selector(keyCommand_showHistory)) ||
      sel_isEqual(action, @selector(keyCommand_voiceSearch)) ||
      sel_isEqual(action, @selector(keyCommand_stop)) ||
      sel_isEqual(action, @selector(keyCommand_showHelp)) ||
      sel_isEqual(action, @selector(keyCommand_showDownloads)) ||
      sel_isEqual(action, @selector(keyCommand_select1)) ||
      sel_isEqual(action, @selector(keyCommand_select2)) ||
      sel_isEqual(action, @selector(keyCommand_select3)) ||
      sel_isEqual(action, @selector(keyCommand_select4)) ||
      sel_isEqual(action, @selector(keyCommand_select5)) ||
      sel_isEqual(action, @selector(keyCommand_select6)) ||
      sel_isEqual(action, @selector(keyCommand_select7)) ||
      sel_isEqual(action, @selector(keyCommand_select8)) ||
      sel_isEqual(action, @selector(keyCommand_select9)) ||
      sel_isEqual(action, @selector(keyCommand_showNextTab)) ||
      sel_isEqual(action, @selector(keyCommand_showPreviousTab))) {
    return self.tabsCount > 0;
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return self.findInPageAvailable;
  }
  if (sel_isEqual(action, @selector(keyCommand_findNext)) ||
      sel_isEqual(action, @selector(keyCommand_findPrevious))) {
    return [self isFindInPageActive];
  }
  if (sel_isEqual(action, @selector(keyCommand_addToBookmarks)) ||
      sel_isEqual(action, @selector(keyCommand_addToReadingList))) {
    return [self isHTTPOrHTTPSPage];
  }
  if (sel_isEqual(action, @selector(keyCommand_reopenLastClosedTab))) {
    sessions::TabRestoreService* const tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            _browser->GetBrowserState());
    return tabRestoreService && !tabRestoreService->entries().empty();
  }
  if (sel_isEqual(action, @selector(keyCommand_reportAnIssue))) {
    return ios::provider::IsUserFeedbackSupported();
  }
  return [super canPerformAction:action withSender:sender];
}

// Changes the title to display the most appropriate string in the shortcut
// menu.
- (void)validateCommand:(UICommand*)command {
  if (command.action == @selector(keyCommand_find)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_FIND_IN_PAGE);
  }
  if (command.action == @selector(keyCommand_select1)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_FIRST_TAB);
  }
  if (command.action == @selector(keyCommand_addToBookmarks)) {
    if ([self isBookmarkedPage]) {
      command.discoverabilityTitle =
          l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_EDIT_BOOKMARK);
    }
  }
  return [super validateCommand:command];
}

#pragma mark - Key Command Actions

- (void)keyCommand_openNewTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewTab"));
  if (_browser->GetBrowserState()->IsOffTheRecord()) {
    [self openNewIncognitoTab];
  } else {
    [self openNewRegularTab];
  }
}

- (void)keyCommand_openNewRegularTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewRegularTab"));
  [self openNewRegularTab];
}

- (void)keyCommand_openNewIncognitoTab {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewIncognitoTab"));
  [self openNewIncognitoTab];
}

- (void)keyCommand_openNewWindow {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewWindow"));
  [_dispatcher openNewWindowWithActivity:ActivityToLoadURL(
                                             WindowActivityKeyCommandOrigin,
                                             GURL(kChromeUINewTabURL))];
}

- (void)keyCommand_openNewIncognitoWindow {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenNewIncognitoWindow"));
  [_dispatcher
      openNewWindowWithActivity:ActivityToLoadURL(
                                    WindowActivityKeyCommandOrigin,
                                    GURL(kChromeUINewTabURL), web::Referrer(),
                                    /* in_incognito */ true)];
}

- (void)keyCommand_reopenLastClosedTab {
  RecordAction(UserMetricsAction("MobileKeyCommandReopenLastClosedTab"));
  ChromeBrowserState* browserState = _browser->GetBrowserState();
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browserState);
  if (!tabRestoreService || tabRestoreService->entries().empty()) {
    return;
  }

  const std::unique_ptr<sessions::TabRestoreService::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  // TODO(crbug.com/1056596) : Support WINDOW restoration under multi-window.
  if (entry->type != sessions::TabRestoreService::TAB) {
    return;
  }

  [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, _browser.get());
}

- (void)keyCommand_find {
  RecordAction(UserMetricsAction("MobileKeyCommandFind"));
  [_dispatcher openFindInPage];
}

- (void)keyCommand_findNext {
  RecordAction(UserMetricsAction("MobileKeyCommandFindNext"));
  [_dispatcher findNextStringInPage];
}

- (void)keyCommand_findPrevious {
  RecordAction(UserMetricsAction("MobileKeyCommandFindPrevious"));
  [_dispatcher findPreviousStringInPage];
}

- (void)keyCommand_openLocation {
  RecordAction(UserMetricsAction("MobileKeyCommandOpenLocation"));
  [_omniboxHandler focusOmnibox];
}

- (void)keyCommand_closeTab {
  RecordAction(UserMetricsAction("MobileKeyCommandCloseTab"));
  [_browserCoordinatorCommandsHandler closeCurrentTab];
}

- (void)keyCommand_showNextTab {
  RecordAction(UserMetricsAction("MobileKeyCommandShowNextTab"));
  WebStateList* webStateList = _browser->GetWebStateList();
  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return;
  }

  // If the active index isn't the last index, activate the next index.
  // (the last index is always `count() - 1`).
  // Otherwise activate the first index.
  if (activeIndex < (webStateList->count() - 1)) {
    webStateList->ActivateWebStateAt(activeIndex + 1);
  } else {
    webStateList->ActivateWebStateAt(0);
  }
}

- (void)keyCommand_showPreviousTab {
  RecordAction(UserMetricsAction("MobileKeyCommandShowPreviousTab"));
  WebStateList* webStateList = _browser->GetWebStateList();
  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex) {
    return;
  }

  // If the active index isn't the first index, activate the prior index.
  // Otherwise index the last index (`count() - 1`).
  if (activeIndex > 0) {
    webStateList->ActivateWebStateAt(activeIndex - 1);
  } else {
    webStateList->ActivateWebStateAt(webStateList->count() - 1);
  }
}

- (void)keyCommand_showBookmarks {
  RecordAction(UserMetricsAction("MobileKeyCommandShowBookmarks"));
  [_browserCoordinatorCommandsHandler showBookmarksManager];
}

- (void)keyCommand_addToBookmarks {
  RecordAction(UserMetricsAction("MobileKeyCommandAddToBookmarks"));
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }
  GURL URL = currentWebState->GetLastCommittedURL();
  if (!URL.is_valid()) {
    return;
  }

  NSString* title = tab_util::GetTabTitle(currentWebState);
  BookmarkAddCommand* command = [[BookmarkAddCommand alloc] initWithURL:URL
                                                                  title:title
                                                   presentFolderChooser:NO];
  [_bookmarksCommandsHandler bookmark:command];
}

- (void)keyCommand_reload {
  RecordAction(UserMetricsAction("MobileKeyCommandReload"));
  self.navigationAgent->Reload();
}

- (void)keyCommand_back {
  RecordAction(UserMetricsAction("MobileKeyCommandBack"));
  if (self.navigationAgent->CanGoBack()) {
    self.navigationAgent->GoBack();
  }
}

- (void)keyCommand_forward {
  RecordAction(UserMetricsAction("MobileKeyCommandForward"));
  if (self.navigationAgent->CanGoForward()) {
    self.navigationAgent->GoForward();
  }
}

- (void)keyCommand_showHistory {
  RecordAction(UserMetricsAction("MobileKeyCommandShowHistory"));
  [_dispatcher showHistory];
}

- (void)keyCommand_voiceSearch {
  RecordAction(UserMetricsAction("MobileKeyCommandVoiceSearch"));
  [LayoutGuideCenterForBrowser(_browser.get())
      referenceView:nil
          underName:kVoiceSearchButtonGuide];
  [_dispatcher startVoiceSearch];
}

- (void)keyCommand_showSettings {
  RecordAction(UserMetricsAction("MobileKeyCommandShowSettings"));
  [_dispatcher showSettingsFromViewController:_viewController];
}

- (void)keyCommand_stop {
  RecordAction(UserMetricsAction("MobileKeyCommandStop"));
  self.navigationAgent->StopLoading();
}

- (void)keyCommand_showHelp {
  RecordAction(UserMetricsAction("MobileKeyCommandShowHelp"));
  [_browserCoordinatorCommandsHandler showHelpPage];
}

- (void)keyCommand_showDownloads {
  RecordAction(UserMetricsAction("MobileKeyCommandShowDownloads"));
  [_browserCoordinatorCommandsHandler showDownloadsFolder];
}

- (void)keyCommand_select1 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowFirstTab"));
  [self showTabAtIndex:0];
}

- (void)keyCommand_select2 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab2"));
  [self showTabAtIndex:1];
}

- (void)keyCommand_select3 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab3"));
  [self showTabAtIndex:2];
}

- (void)keyCommand_select4 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab4"));
  [self showTabAtIndex:3];
}

- (void)keyCommand_select5 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab5"));
  [self showTabAtIndex:4];
}

- (void)keyCommand_select6 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab6"));
  [self showTabAtIndex:5];
}

- (void)keyCommand_select7 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab7"));
  [self showTabAtIndex:6];
}

- (void)keyCommand_select8 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowTab8"));
  [self showTabAtIndex:7];
}

- (void)keyCommand_select9 {
  RecordAction(UserMetricsAction("MobileKeyCommandShowLastTab"));
  [self showTabAtIndex:self.tabsCount - 1];
}

- (void)keyCommand_reportAnIssue {
  RecordAction(UserMetricsAction("MobileKeyCommandReportAnIssue"));
  [_dispatcher
      showReportAnIssueFromViewController:_viewController
                                   sender:UserFeedbackSender::KeyCommand];
}

- (void)keyCommand_addToReadingList {
  RecordAction(UserMetricsAction("MobileKeyCommandAddToReadingList"));
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }
  GURL URL = currentWebState->GetLastCommittedURL();
  if (!URL.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  NSString* title = tab_util::GetTabTitle(currentWebState);
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:URL title:title];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(_browser.get());
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)keyCommand_showReadingList {
  RecordAction(UserMetricsAction("MobileKeyCommandShowReadingList"));
  [_browserCoordinatorCommandsHandler showReadingList];
}

- (void)keyCommand_goToTabGrid {
  RecordAction(UserMetricsAction("MobileKeyCommandGoToTabGrid"));
  [_dispatcher prepareTabSwitcher];
  [_dispatcher displayTabSwitcherInGridLayout];
}

- (void)keyCommand_clearBrowsingData {
  RecordAction(UserMetricsAction("MobileKeyCommandClearBrowsingData"));
  [_dispatcher showClearBrowsingDataSettings];
}

#pragma mark - Private

- (WebNavigationBrowserAgent*)navigationAgent {
  return WebNavigationBrowserAgent::FromBrowser(_browser.get());
}

- (BOOL)isFindInPageAvailable {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (BOOL)isFindInPageActive {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(currentWebState);
  return (helper && helper->IsFindUIActive());
}

- (NSUInteger)tabsCount {
  return _browser->GetWebStateList()->count();
}

- (BOOL)isEditingText {
  UIResponder* firstResponder = GetFirstResponder();
  return [firstResponder isKindOfClass:[UITextField class]] ||
         [firstResponder isKindOfClass:[UITextView class]] ||
         [[KeyboardObserverHelper sharedKeyboardObserver] isKeyboardVisible];
}

- (void)openNewRegularTab {
  OpenNewTabCommand* newTabCommand = [OpenNewTabCommand command];
  newTabCommand.shouldFocusOmnibox = YES;
  [_dispatcher openURLInNewTab:newTabCommand];
}

- (void)openNewIncognitoTab {
  OpenNewTabCommand* newIncognitoTabCommand =
      [OpenNewTabCommand incognitoTabCommand];
  newIncognitoTabCommand.shouldFocusOmnibox = YES;
  [_dispatcher openURLInNewTab:newIncognitoTabCommand];
}

- (void)showTabAtIndex:(NSUInteger)index {
  WebStateList* webStateList = _browser->GetWebStateList();
  if (webStateList->ContainsIndex(index)) {
    webStateList->ActivateWebStateAt(static_cast<int>(index));
  }
}

- (BOOL)isHTTPOrHTTPSPage {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  const GURL& url = currentWebState->GetLastCommittedURL();
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

- (BOOL)isBookmarkedPage {
  web::WebState* currentWebState =
      _browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  const GURL& url = currentWebState->GetLastCommittedURL();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          _browser->GetBrowserState());
  return bookmarkModel->IsBookmarked(url);
}

@end
