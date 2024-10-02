// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"

#import "base/command_line.h"
#import "base/containers/contains.h"
#import "base/files/file_util.h"
#import "base/ios/ios_util.h"
#import "base/json/json_string_value_serializer.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/threading/thread_restrictions.h"
#import "base/values.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/metrics/demographics/demographic_metrics_provider.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "components/sync/base/features.h"
#import "components/sync/base/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "components/variations/variations_associated_data.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/default_browser/utils_test_support.h"
#import "ios/chrome/browser/main/browser_provider.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/browsing_data_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/window_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/testing/hardware_keyboard_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/testing/open_url_context.h"
#import "ios/testing/verify_custom_webkit.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/url_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "services/metrics/public/cpp/ukm_recorder.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Returns a JSON-encoded string representing the given `pref`. If `pref` is
// nullptr, returns a string representing a base::Value of type NONE.
NSString* SerializedPref(const PrefService::Preference* pref) {
  base::Value none_value(base::Value::Type::NONE);

  const base::Value* value = pref ? pref->GetValue() : &none_value;
  DCHECK(value);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*value);
  return base::SysUTF8ToNSString(serialized_value);
}
// Returns a JSON-encoded string representing the given `value`. If `value` is
// nullptr, returns a string representing a base::Value of type NONE.
NSString* SerializedValue(const base::Value* value) {
  base::Value none_value(base::Value::Type::NONE);
  const base::Value* result = value ? value : &none_value;
  DCHECK(result);

  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(*result);
  return base::SysUTF8ToNSString(serialized_value);
}
}

@implementation JavaScriptExecutionResult

- (instancetype)initWithResult:(NSString*)result
           successfulExecution:(BOOL)outcome {
  self = [super init];
  if (self) {
    _result = result;
    _success = outcome;
  }
  return self;
}
@end

@implementation ChromeEarlGreyAppInterface

+ (BOOL)isRTL {
  return UseRTLLayout();
}

+ (NSError*)clearBrowsingHistory {
  if (chrome_test_util::ClearBrowsingHistory()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser history timed out");
}

+ (NSInteger)browsingHistoryEntryCountWithError:
    (NSError* __autoreleasing*)error {
  return chrome_test_util::GetBrowsingHistoryEntryCount(error);
}

+ (NSInteger)navigationBackListItemsCount {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  if (!webState)
    return -1;

  return webState->GetNavigationManager()->GetBackwardItems().size();
}

+ (NSError*)removeBrowsingCache {
  if (chrome_test_util::RemoveBrowsingCache()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser cache for main tabs timed out");
}

+ (void)saveSessionImmediately {
  SessionRestorationBrowserAgent::FromBrowser(
      chrome_test_util::GetMainBrowser())
      ->SaveSession(/*immediately=*/true);
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  ProceduralBlock completionBlock = ^{
    dispatch_semaphore_signal(semaphore);
  };
  [[SessionServiceIOS sharedService] shutdownWithCompletion:completionBlock];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
}

+ (NSError*)clearAllWebStateBrowsingData {
  if (chrome_test_util::ClearAllWebStateBrowsingData()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing web state browsing data for main tabs timed out");
}

+ (void)sceneOpenURL:(NSString*)spec {
  NSURL* url = [NSURL URLWithString:spec];
  TestOpenURLContext* context = [[TestOpenURLContext alloc] init];
  context.URL = url;

  UIApplication* application = UIApplication.sharedApplication;
  UIScene* scene = application.connectedScenes.anyObject;

  [scene.delegate scene:scene openURLContexts:[NSSet setWithObject:context]];
}

+ (void)startLoadingURL:(NSString*)spec {
  chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(spec)));
}

+ (bool)isLoading {
  return chrome_test_util::IsLoading();
}

+ (void)startReloading {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->Reload();
}

+ (void)openURLFromExternalApp:(NSString*)URL {
  chrome_test_util::OpenChromeFromExternalApp(
      GURL(base::SysNSStringToUTF8(URL)));
}

+ (void)dismissSettings {
  [chrome_test_util::HandlerForActiveBrowser() closeSettingsUI];
}

#pragma mark - Tab Utilities (EG2)

+ (void)selectTabAtIndex:(NSUInteger)index {
  chrome_test_util::SelectTabAtIndexInCurrentMode(index);
}

+ (BOOL)isIncognitoMode {
  return chrome_test_util::IsIncognitoMode();
}

+ (void)closeTabAtIndex:(NSUInteger)index {
  chrome_test_util::CloseTabAtIndex(index);
}

+ (NSUInteger)mainTabCount {
  return chrome_test_util::GetMainTabCount();
}

+ (NSUInteger)incognitoTabCount {
  return chrome_test_util::GetIncognitoTabCount();
}

+ (NSUInteger)browserCount {
  return chrome_test_util::RegularBrowserCount();
}

+ (NSUInteger)evictedMainTabCount {
  return chrome_test_util::GetEvictedMainTabCount();
}

+ (void)evictOtherBrowserTabs {
  chrome_test_util::EvictOtherBrowserTabs();
}

+ (NSError*)simulateTabsBackgrounding {
  if (!chrome_test_util::SimulateTabsBackgrounding()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to simulate tab backgrounding.");
  }
  return nil;
}

+ (NSError*)setCurrentTabsToBeColdStartTabs {
  if (!chrome_test_util::SetCurrentTabsToBeColdStartTabs()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to state tabs as cold start tabs");
  }
  return nil;
}

+ (NSError*)resetTabUsageRecorder {
  if (!chrome_test_util::ResetTabUsageRecorder()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to reset the TabUsageRecorder");
  }
  return nil;
}

+ (void)openNewTab {
  chrome_test_util::OpenNewTab();
}

+ (void)simulateExternalAppURLOpeningWithURL:(NSURL*)URL {
  chrome_test_util::SimulateExternalAppURLOpeningWithURL(URL);
}

+ (void)simulateAddAccountFromWeb {
  chrome_test_util::SimulateAddAccountFromWeb();
}

+ (void)closeCurrentTab {
  chrome_test_util::CloseCurrentTab();
}

+ (void)openNewIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
}

+ (NSString*)currentTabTitle {
  return chrome_test_util::GetCurrentTabTitle();
}

+ (NSString*)nextTabTitle {
  return chrome_test_util::GetNextTabTitle();
}

+ (void)closeAllTabsInCurrentMode {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

+ (NSError*)closeAllNormalTabs {
  bool success = chrome_test_util::CloseAllNormalTabs();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Could not close all normal tabs");
  }
  return nil;
}

+ (NSError*)closeAllIncognitoTabs {
  bool success = chrome_test_util::CloseAllIncognitoTabs();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Could not close all Incognito tabs");
  }
  return nil;
}

+ (void)closeAllTabs {
  chrome_test_util::CloseAllTabs();
}

+ (void)startGoingBack {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->GoBack();
}

+ (void)startGoingForward {
  WebNavigationBrowserAgent::FromBrowser(chrome_test_util::GetMainBrowser())
      ->GoForward();
}

+ (NSString*)currentTabID {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetStableIdentifier();
}

+ (NSString*)nextTabID {
  web::WebState* web_state = chrome_test_util::GetNextWebState();
  return web_state->GetStableIdentifier();
}

+ (NSUInteger)indexOfActiveNormalTab {
  return chrome_test_util::GetIndexOfActiveNormalTab();
}

#pragma mark - Window utilities (EG2)

+ (UIWindow*)windowWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return state.window;
    }
  }
  return nil;
}

// Returns screen position of the given `windowNumber`
+ (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return
          [state.window convertRect:state.window.frame
                  toCoordinateSpace:state.window.screen.fixedCoordinateSpace];
    }
  }
  return CGRectZero;
}

+ (NSUInteger)windowCount [[nodiscard]] {
  // If the scene API is in use, return the count of open sessions.
  return UIApplication.sharedApplication.openSessions.count;
}

+ (NSUInteger)foregroundWindowCount [[nodiscard]] {
  // If the scene API is in use, look at all the connected scenes and count
  // those in the foreground.
  NSUInteger count = 0;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (scene.activationState == UISceneActivationStateForegroundActive ||
        scene.activationState == UISceneActivationStateForegroundInactive) {
      count++;
    }
  }
  return count;
}

+ (NSError*)openNewWindow {
  if (!base::ios::IsMultipleScenesSupported()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Multiwindow not supported");
  }

  // Always disable default browser promo in new window, to avoid
  // messages to be closed too early.
  [self disableDefaultBrowserPromo];

  NSUserActivity* activity =
      [[NSUserActivity alloc] initWithActivityType:@"EG2NewWindow"];
  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  [UIApplication.sharedApplication
      requestSceneSessionActivation:nil /* make a new scene */
                       userActivity:activity
                            options:options
                       errorHandler:nil];
  return nil;
}

+ (void)openNewTabInWindowWithNumber:(int)windowNumber {
  chrome_test_util::OpenNewTabInWindowWithNumber(windowNumber);
}

+ (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  NSString* newAccessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)newWindowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      state.window.accessibilityIdentifier = newAccessibilityIdentifier;
      break;
    }
  }
}

+ (void)closeWindowWithNumber:(int)windowNumber {
  NSArray<SceneState*>* connectedScenes =
      chrome_test_util::GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connectedScenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      UIWindowSceneDestructionRequestOptions* options =
          [[UIWindowSceneDestructionRequestOptions alloc] init];
      options.windowDismissalAnimation =
          UIWindowSceneDismissalAnimationStandard;
      [UIApplication.sharedApplication
          requestSceneSessionDestruction:state.scene.session
                                 options:options
                            errorHandler:nil];
    }
  }
}

+ (void)closeAllExtraWindows {
  if (!base::ios::IsMultipleScenesSupported())
    return;
  SceneState* foreground_scene_state =
      chrome_test_util::GetMainController().appState.foregroundActiveScene;
  // New windows get an accessibilityIdentifier equal to the number of windows
  // when they are created.
  // Renumber the remaining window to avoid conflicts with future windows.
  foreground_scene_state.window.accessibilityIdentifier = @"0";
  NSSet<UISceneSession*>* sessions =
      UIApplication.sharedApplication.openSessions;
  if (sessions.count <= 1)
    return;
  for (UISceneSession* session in sessions) {
    if (foreground_scene_state.scene == session.scene) {
      continue;
    }
    UIWindowSceneDestructionRequestOptions* options =
        [[UIWindowSceneDestructionRequestOptions alloc] init];
    options.windowDismissalAnimation = UIWindowSceneDismissalAnimationStandard;
    [UIApplication.sharedApplication requestSceneSessionDestruction:session
                                                            options:options
                                                       errorHandler:nil];
  }
}

+ (void)startLoadingURL:(NSString*)spec inWindowWithNumber:(int)windowNumber {
  chrome_test_util::LoadUrlInWindowWithNumber(
      GURL(base::SysNSStringToUTF8(spec)), windowNumber);
}

+ (BOOL)isLoadingInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::IsLoadingInWindowWithNumber(windowNumber);
}

+ (BOOL)webStateContainsText:(NSString*)text
          inWindowWithNumber:(int)windowNumber {
  web::WebState* webState =
      chrome_test_util::GetCurrentWebStateForWindowWithNumber(windowNumber);
  return webState ? web::test::IsWebViewContainingText(
                        webState, base::SysNSStringToUTF8(text))
                  : NO;
}

+ (NSUInteger)mainTabCountInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::GetMainTabCountForWindowWithNumber(windowNumber);
}

+ (NSUInteger)incognitoTabCountInWindowWithNumber:(int)windowNumber {
  return chrome_test_util::GetIncognitoTabCountForWindowWithNumber(
      windowNumber);
}

+ (UIWindow*)keyWindow {
  NSSet<UIScene*>* scenes = UIApplication.sharedApplication.connectedScenes;
  for (UIScene* scene in scenes) {
    UIWindowScene* windowScene =
        base::mac::ObjCCastStrict<UIWindowScene>(scene);

    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow) {
        return window;
      }
    }
  }

  return nil;
}

#pragma mark - WebState Utilities (EG2)

+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID {
  bool success = web::test::TapWebViewElementWithIdInIframe(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID));
  if (!success) {
    return testing::NSErrorWithLocalizedDescription([NSString
        stringWithFormat:@"Failed to tap element with ID=%@", elementID]);
  }

  return nil;
}

+ (NSError*)tapWebStateElementWithID:(NSString*)elementID {
  NSError* error = nil;
  bool success = web::test::TapWebViewElementWithId(
      chrome_test_util::GetCurrentWebState(),
      base::SysNSStringToUTF8(elementID), &error);
  if (!success || error) {
    NSString* errorDescription =
        [NSString stringWithFormat:
                      @"Failed to tap web state element with ID=%@! Error: %@",
                      elementID, error];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state containing element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateNotContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state without element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)waitForWebStateContainingTextInIFrame:(NSString*)text {
  std::string stringText = base::SysNSStringToUTF8(text);
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web::test::IsWebViewContainingTextInFrame(
        chrome_test_util::GetCurrentWebState(), stringText);
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:
            @"Failed waiting for web state's iframes containing text %@", text];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (NSError*)submitWebStateFormWithID:(NSString*)formID {
  bool success = web::test::SubmitWebViewFormWithId(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(formID));

  if (!success) {
    NSString* errorString =
        [NSString stringWithFormat:@"Failed to submit form with ID=%@", formID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (BOOL)webStateContainsElement:(ElementSelector*)selector {
  return web::test::IsWebViewContainingElement(
      chrome_test_util::GetCurrentWebState(), selector);
}

+ (BOOL)webStateContainsText:(NSString*)text {
  return web::test::IsWebViewContainingText(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
}

+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_LOADED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view loaded image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      base::SysNSStringToUTF8(imageID), chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_BLOCKED);

  if (!success) {
    NSString* errorString = [NSString
        stringWithFormat:@"Failed waiting for web view blocked image %@",
                         imageID];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return nil;
}

+ (NSError*)waitForWebStateZoomScale:(CGFloat)scale {
  bool success = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    web::WebState* web_state = chrome_test_util::GetCurrentWebState();
    if (!web_state) {
      return false;
    }

    CGFloat current_scale =
        [[web_state->GetWebViewProxy() scrollViewProxy] zoomScale];
    return (current_scale > (scale - 0.05)) && (current_scale < (scale + 0.05));
  });
  if (!success) {
    NSString* NSErrorDescription = [NSString
        stringWithFormat:@"Failed waiting for web state zoom scale %f", scale];
    return testing::NSErrorWithLocalizedDescription(NSErrorDescription);
  }
  return nil;
}

+ (void)signOutAndClearIdentities {
  chrome_test_util::SignOutAndClearIdentities();
}

+ (BOOL)hasIdentities {
  return chrome_test_util::HasIdentities();
}

+ (NSString*)webStateVisibleURL {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetCurrentWebState()->GetVisibleURL().spec());
}

+ (NSString*)webStateLastCommittedURL {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetCurrentWebState()->GetLastCommittedURL().spec());
}

+ (NSError*)purgeCachedWebViewPages {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  web_state->SetWebUsageEnabled(false);
  if (!chrome_test_util::RemoveBrowsingCache()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Fail to purge cached web view pages.");
  }

  // Attempt to deflake WebKit sometimes still holding on to the browser cache
  // with a larger hammer.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath library_dir = base::mac::GetUserLibraryPath();
  base::FilePath webkit_cache_dir = library_dir.Append("WebKit");
  DeletePathRecursively(webkit_cache_dir);

  web_state->SetWebUsageEnabled(true);
  web_state->GetNavigationManager()->LoadIfNecessary();

  return nil;
}

+ (BOOL)isRestoreSessionInProgress {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetNavigationManager()->IsRestoreSessionInProgress();
}

+ (BOOL)webStateWebViewUsesContentInset {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return web_state->GetWebViewProxy().shouldUseViewContentInset;
}

+ (CGSize)webStateWebViewSize {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return [web_state->GetWebViewProxy() bounds].size;
}

+ (void)stopAllWebStatesLoading {
  WebStateList* web_state_list =
      chrome_test_util::GetMainController()
          .browserProviderInterface.currentBrowserProvider.browser
          ->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    if (web_state->IsLoading()) {
      web_state->Stop();
    }
  }
}

#pragma mark - Bookmarks Utilities (EG2)

+ (NSError*)waitForBookmarksToFinishinLoading {
  bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return chrome_test_util::BookmarksLoaded();
  });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model did not load");
  }
  return nil;
}

+ (NSError*)clearBookmarks {
  bool success = chrome_test_util::ClearBookmarks();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Not all bookmarks were removed.");
  }
  return nil;
}

#pragma mark - URL Utilities (EG2)

+ (NSString*)displayTitleForURL:(NSString*)URL {
  return base::SysUTF16ToNSString(
      web::GetDisplayTitleForUrl(GURL(base::SysNSStringToUTF8(URL))));
}

#pragma mark - Sync Utilities (EG2)

+ (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return chrome_test_util::GetNumberOfSyncEntities(type);
}

+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title {
  chrome_test_util::AddBookmarkToFakeSyncServer(base::SysNSStringToUTF8(URL),
                                                base::SysNSStringToUTF8(title));
}

+ (void)addFakeSyncServerLegacyBookmarkWithURL:(NSString*)URL
                                         title:(NSString*)title
                     originator_client_item_id:
                         (NSString*)originator_client_item_id {
  chrome_test_util::AddLegacyBookmarkToFakeSyncServer(
      base::SysNSStringToUTF8(URL), base::SysNSStringToUTF8(title),
      base::SysNSStringToUTF8(originator_client_item_id));
}

+ (void)addFakeSyncServerTypedURL:(NSString*)URL {
  chrome_test_util::AddTypedURLToFakeSyncServer(base::SysNSStringToUTF8(URL));
}

+ (void)addFakeSyncServerHistoryVisit:(NSURL*)URL {
  chrome_test_util::AddHistoryVisitToFakeSyncServer(net::GURLWithNSURL(URL));
}

+ (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp {
  chrome_test_util::AddDeviceInfoToFakeSyncServer(
      base::SysNSStringToUTF8(deviceName), lastUpdatedTimestamp);
}

+ (void)addHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::AddTypedURLToClient(GURL(base::SysNSStringToUTF8(URL)));
}

+ (void)deleteHistoryServiceTypedURL:(NSString*)URL {
  chrome_test_util::DeleteTypedUrlFromClient(
      GURL(base::SysNSStringToUTF8(URL)));
}

+ (BOOL)isURL:(NSString*)spec presentOnClient:(BOOL)expectPresent {
  NSError* error = nil;
  GURL URL(base::SysNSStringToUTF8(spec));
  BOOL success =
      chrome_test_util::IsUrlPresentOnClient(URL, expectPresent, &error);
  return success && !error;
}

+ (void)triggerSyncCycleForType:(syncer::ModelType)type {
  chrome_test_util::TriggerSyncCycle(type);
}

+ (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender {
  chrome_test_util::AddUserDemographicsToSyncServer(rawBirthYear, gender);
}

+ (void)clearAutofillProfileWithGUID:(NSString*)GUID {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  chrome_test_util::ClearAutofillProfile(utfGUID);
}

+ (void)addAutofillProfileToFakeSyncServerWithGUID:(NSString*)GUID
                               autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  chrome_test_util::AddAutofillProfileToFakeSyncServer(utfGUID, utfFullName);
}

+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName {
  std::string utfGUID = base::SysNSStringToUTF8(GUID);
  std::string utfFullName = base::SysNSStringToUTF8(fullName);
  return chrome_test_util::IsAutofillProfilePresent(utfGUID, utfFullName);
}

+ (void)deleteAutofillProfileFromFakeSyncServerWithGUID:(NSString*)GUID {
  chrome_test_util::DeleteAutofillProfileFromFakeSyncServer(
      base::SysNSStringToUTF8(GUID));
}

+ (void)signInWithoutSyncWithIdentity:(FakeSystemIdentity*)identity {
  chrome_test_util::SignInWithoutSync(identity);
}

+ (void)clearSyncServerData {
  chrome_test_util::ClearSyncServerData();
}

+ (NSError*)waitForSyncEngineInitialized:(BOOL)isInitialized
                             syncTimeout:(base::TimeDelta)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    return chrome_test_util::IsSyncEngineInitialized() == isInitialized;
  });
  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Sync must be initialized: %@",
                                   isInitialized ? @"YES" : @"NO"];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSError*)waitForSyncFeatureEnabled:(BOOL)isEnabled
                          syncTimeout:(base::TimeDelta)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    ChromeBrowserState* browser_state =
        chrome_test_util::GetOriginalBrowserState();
    DCHECK(browser_state);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(browser_state);
    return syncService->IsSyncFeatureEnabled() == isEnabled;
  });
  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Sync feature must be enabled: %@",
                                   isEnabled ? @"YES" : @"NO"];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSError*)waitForSyncTransportStateActiveWithTimeout:
    (base::TimeDelta)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    ChromeBrowserState* browser_state =
        chrome_test_util::GetOriginalBrowserState();
    DCHECK(browser_state);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(browser_state);
    return syncService->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE;
  });
  if (!success) {
    ChromeBrowserState* browser_state =
        chrome_test_util::GetOriginalBrowserState();
    NSString* errorDescription = [NSString
        stringWithFormat:
            @"Sync transport must be active, but actual state was: %d",
            (int)SyncServiceFactory::GetForBrowserState(browser_state)
                ->GetTransportState()];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

+ (NSString*)syncCacheGUID {
  return base::SysUTF8ToNSString(chrome_test_util::GetSyncCacheGuid());
}

+ (NSError*)waitForSyncInvalidationFields {
  const bool success = WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return chrome_test_util::VerifySyncInvalidationFieldsPopulated();
  });
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"The local DeviceInfo doesn't have invalidation fields");
  }
  return nil;
}

+ (BOOL)isFakeSyncServerSetUp {
  return chrome_test_util::IsFakeSyncServerSetUp();
}

+ (void)setUpFakeSyncServer {
  chrome_test_util::SetUpFakeSyncServer();
}

+ (void)tearDownFakeSyncServer {
  chrome_test_util::TearDownFakeSyncServer();
}

+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count {
  std::string UTF8Name = base::SysNSStringToUTF8(name);
  NSError* __autoreleasing tempError = nil;
  bool success = chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
      (syncer::ModelType)type, UTF8Name, count, &tempError);
  NSError* error = tempError;

  if (!success and !error) {
    NSString* errorString =
        [NSString stringWithFormat:@"Expected %zu entities of the %d type.",
                                   count, (syncer::ModelType)type];
    return testing::NSErrorWithLocalizedDescription(errorString);
  }

  return error;
}

+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs {
  std::multiset<std::string> multisetSpecs;
  for (NSString* spec in specs) {
    multisetSpecs.insert(base::SysNSStringToUTF8(spec));
  }

  NSError* __autoreleasing tempError = nil;
  bool success =
      chrome_test_util::VerifySessionsOnSyncServer(multisetSpecs, &tempError);
  NSError* error = tempError;
  if (!success && !error) {
    error = testing::NSErrorWithLocalizedDescription(
        @"Error occurred during verification sessions.");
  }
  return error;
}

+ (NSError*)verifyHistoryOnSyncServerWithURLs:(NSArray<NSURL*>*)URLs {
  std::multiset<GURL> multisetUrls;
  for (NSURL* url in URLs) {
    multisetUrls.insert(net::GURLWithNSURL(url));
  }

  NSError* __autoreleasing tempError = nil;
  bool success =
      chrome_test_util::VerifyHistoryOnSyncServer(multisetUrls, &tempError);
  NSError* error = tempError;
  if (!success && !error) {
    error = testing::NSErrorWithLocalizedDescription(
        @"Error occurred during verifying history URLs.");
  }
  return error;
}

+ (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase {
  chrome_test_util::AddBookmarkWithSyncPassphrase(
      base::SysNSStringToUTF8(syncPassphrase));
}

#pragma mark - JavaScript Utilities (EG2)

+ (JavaScriptExecutionResult*)executeJavaScript:(NSString*)javaScript {
  __block web::WebFrame* main_frame = nullptr;
  bool completed =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        main_frame = chrome_test_util::GetCurrentWebState()
                         ->GetPageWorldWebFramesManager()
                         ->GetMainWebFrame();
        return main_frame != nullptr;
      });

  if (!main_frame) {
    DLOG(ERROR) << "No main web frame exists.";
    return [[JavaScriptExecutionResult alloc] initWithResult:nil
                                         successfulExecution:NO];
  }

  std::u16string script = base::SysNSStringToUTF16(javaScript);
  __block bool handlerCalled = false;
  __block NSString* blockResult = nil;
  __block NSError* blockError = nil;
  main_frame->ExecuteJavaScript(
      script, base::BindOnce(^(const base::Value* value, NSError* error) {
        handlerCalled = true;
        blockError = error;
        blockResult = SerializedValue(value);
      }));

  completed = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return handlerCalled;
  });

  DLOG_IF(ERROR, !completed) << "JavaScript execution timed out.";
  DLOG_IF(ERROR, blockError) << "JavaScript execution of:\n"
                             << script << "\nfailed with error:\n"
                             << base::SysNSStringToUTF8(blockError.description);

  BOOL success = completed && !blockError;
  JavaScriptExecutionResult* result =
      [[JavaScriptExecutionResult alloc] initWithResult:blockResult
                                    successfulExecution:success];
  return result;
}

+ (NSString*)mobileUserAgentString {
  return base::SysUTF8ToNSString(
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
}

#pragma mark - Accessibility Utilities (EG2)

+ (NSError*)verifyAccessibilityForCurrentScreen {
  NSError* error = nil;
  BOOL success = chrome_test_util::VerifyAccessibilityForCurrentScreen(&error);
  if (!success || error) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Accessibility checks failed! Error: %@", error];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

#pragma mark - Check features (EG2)

+ (BOOL)isVariationEnabled:(int)variationID {
  variations::VariationsIdsProvider* provider =
      variations::VariationsIdsProvider::GetInstance();
  std::vector<variations::VariationID> ids = provider->GetVariationsVector(
      {variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
       variations::GOOGLE_WEB_PROPERTIES_FIRST_PARTY});
  return base::Contains(ids, variationID);
}

+ (BOOL)isTriggerVariationEnabled:(int)variationID {
  variations::VariationsIdsProvider* provider =
      variations::VariationsIdsProvider::GetInstance();
  std::vector<variations::VariationID> ids = provider->GetVariationsVector(
      {variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
       variations::GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY});
  return base::Contains(ids, variationID);
}

+ (BOOL)isUKMEnabled {
  return base::FeatureList::IsEnabled(ukm::kUkmFeature);
}

+ (BOOL)isSynthesizedRestoreSessionEnabled {
  return base::FeatureList::IsEnabled(
      web::features::kSynthesizedRestoreSession);
}

+ (BOOL)isTestFeatureEnabled {
  return base::FeatureList::IsEnabled(kTestFeature);
}

+ (BOOL)isDemographicMetricsReportingEnabled {
  return base::FeatureList::IsEnabled(metrics::kDemographicMetricsReporting);
}

+ (BOOL)isSyncHistoryDataTypeEnabled {
  return base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType);
}

+ (BOOL)appHasLaunchSwitch:(NSString*)launchSwitch {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      base::SysNSStringToUTF8(launchSwitch));
}

+ (BOOL)isCustomWebKitLoadedIfRequested {
  return IsCustomWebKitLoadedIfRequested();
}

+ (BOOL)isLoadSimulatedRequestAPIEnabled {
  return web::features::IsLoadSimulatedRequestAPIEnabled();
}

+ (BOOL)isMobileModeByDefault {
  web::UserAgentType webClientUserAgent =
      web::GetWebClient()->GetDefaultUserAgent(
          chrome_test_util::GetCurrentWebState(), GURL());

  return webClientUserAgent == web::UserAgentType::MOBILE;
}

+ (BOOL)areMultipleWindowsSupported {
  return base::ios::IsMultipleScenesSupported();
}

+ (BOOL)isNewOverflowMenuEnabled {
  return IsNewOverflowMenuEnabled();
}

+ (BOOL)isUseLensToSearchForImageEnabled {
  TemplateURLService* service =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  return base::FeatureList::IsEnabled(kUseLensToSearchForImage) &&
         ios::provider::IsLensSupported() &&
         ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET &&
         search_engines::SupportsSearchImageWithLens(service);
}

+ (BOOL)isThumbstripEnabledForWindowWithNumber:(int)windowNumber {
  return ShowThumbStripInTraitCollection(
      [self windowWithNumber:windowNumber].traitCollection);
}

+ (BOOL)isWebChannelsEnabled {
  return base::FeatureList::IsEnabled(kEnableWebChannels);
}

+ (BOOL)isUIButtonConfigurationEnabled {
  return IsUIButtonConfigurationEnabled();
}
#pragma mark - ContentSettings

+ (ContentSetting)popupPrefValue {
  return ios::HostContentSettingsMapFactory::GetForBrowserState(
             chrome_test_util::GetOriginalBrowserState())
      ->GetDefaultContentSetting(ContentSettingsType::POPUPS, NULL);
}

+ (void)setPopupPrefValue:(ContentSetting)value {
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS, value);
}

+ (void)resetDesktopContentSetting {
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetDefaultContentSetting(ContentSettingsType::REQUEST_DESKTOP_SITE,
                                 CONTENT_SETTING_BLOCK);
}

#pragma mark - Default Utilities (EG2)

+ (void)setUserDefaultObject:(id)value forKey:(NSString*)defaultName {
  [[NSUserDefaults standardUserDefaults] setObject:value forKey:defaultName];
}

+ (void)removeUserDefaultObjectForKey:(NSString*)key {
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:key];
}

#pragma mark - Pref Utilities (EG2)

+ (NSString*)localStatePrefValue:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  const PrefService::Preference* pref =
      GetApplicationContext()->GetLocalState()->FindPreference(path);
  return SerializedPref(pref);
}

+ (void)setIntegerValue:(int)value forLocalStatePref:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetInteger(path, value);
}

+ (NSString*)userPrefValue:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  const PrefService::Preference* pref =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs()->FindPreference(
          path);
  return SerializedPref(pref);
}

+ (void)setBoolValue:(BOOL)value forUserPref:(NSString*)prefName {
  chrome_test_util::SetBooleanUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      base::SysNSStringToUTF8(prefName).c_str(), value);
}

+ (void)setIntegerValue:(int)value forUserPref:(NSString*)prefName {
  chrome_test_util::SetIntegerUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      base::SysNSStringToUTF8(prefName).c_str(), value);
}

+ (void)clearUserPrefWithName:(NSString*)prefName {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->ClearPref(base::SysNSStringToUTF8(prefName));
}

+ (void)commitPendingUserPrefsWrite {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->CommitPendingWrite();
}

+ (void)resetBrowsingDataPrefs {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->ClearPref(browsing_data::prefs::kDeleteBrowsingHistory);
  prefs->ClearPref(browsing_data::prefs::kDeleteCookies);
  prefs->ClearPref(browsing_data::prefs::kDeleteCache);
  prefs->ClearPref(browsing_data::prefs::kDeletePasswords);
  prefs->ClearPref(browsing_data::prefs::kDeleteFormData);
}

+ (void)resetDataForLocalStatePref:(NSString*)prefName {
  std::string path = base::SysNSStringToUTF8(prefName);
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->ClearPref(path);
}

#pragma mark - Unified Consent utilities

+ (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled {
  UnifiedConsentServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetUrlKeyedAnonymizedDataCollectionEnabled(enabled);
}

#pragma mark - Keyboard Command Utilities

+ (NSInteger)registeredKeyCommandCount {
  UIViewController* browserViewController =
      chrome_test_util::GetMainController()
          .browserProviderInterface.mainBrowserProvider.viewController;
  // The BVC delegates its key commands to its next responder,
  // KeyCommandsProvider.
  return browserViewController.nextResponder.keyCommands.count;
}

+ (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags {
  chrome_test_util::SimulatePhysicalKeyboardEvent(flags, input);
}

#pragma mark - Pasteboard utilities

+ (void)clearPasteboardURLs {
  [[UIPasteboard generalPasteboard] setURLs:nil];
}

+ (void)clearPasteboard {
  [[UIPasteboard generalPasteboard] setItems:@[]];
}

+ (BOOL)pasteboardHasImages {
  return [UIPasteboard.generalPasteboard hasImages];
}

+ (NSArray<NSString*>*)pasteboardStrings {
  return [UIPasteboard generalPasteboard].strings;
}

+ (NSString*)pasteboardURLSpec {
  return [UIPasteboard generalPasteboard].URL.absoluteString;
}

+ (void)copyTextToPasteboard:(NSString*)text {
  [UIPasteboard.generalPasteboard setString:text];
}

#pragma mark - Watcher utilities

// Delay between two watch cycles.
constexpr base::TimeDelta kWatcherCycleDelay = base::Milliseconds(200);

// Set of buttons being watched for.
NSMutableSet* watchingButtons;
// Set of buttons that were actually watched.
NSMutableSet* watchedButtons;

// Current watch number, to allow terminating older scheduled runs.
int watchRunNumber = 0;

+ (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(base::TimeDelta)timeout {
  watchRunNumber++;
  watchedButtons = [NSMutableSet set];
  watchingButtons = [NSMutableSet set];
  for (NSString* label in labels) {
    [watchingButtons addObject:label];
  }
  [self scheduleNextWatchForButtonsWithTimeout:timeout
                                     runNumber:watchRunNumber];
}

+ (BOOL)watcherDetectedButtonWithLabel:(NSString*)label {
  return [watchedButtons containsObject:label];
}

+ (void)stopWatcher {
  [watchingButtons removeAllObjects];
  [watchedButtons removeAllObjects];
}

// Schedule the next watch cycles from a background thread, that will dispatch
// the actual check, async, on the main thread, since UI objects can only
// be accessed from there. Scheduling directly on the main
// thread would let EG to try to drain the main thread queue without
// success.
+ (void)scheduleNextWatchForButtonsWithTimeout:(base::TimeDelta)timeout
                                     runNumber:(int)runNumber {
  dispatch_queue_t background_queue =
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kWatcherCycleDelay.InNanoseconds()),
      background_queue, ^{
        dispatch_async(dispatch_get_main_queue(), ^{
          if (!watchingButtons.count || runNumber != watchRunNumber)
            return;

          NSMutableArray<UIWindow*>* windows = [[NSMutableArray alloc] init];
          for (UIScene* scene in UIApplication.sharedApplication
                   .connectedScenes) {
            UIWindowScene* windowScene =
                base::mac::ObjCCastStrict<UIWindowScene>(scene);
            [windows addObjectsFromArray:windowScene.windows];
          }

          [self findButtonsWithLabelsInViews:windows];

          if (watchingButtons.count && timeout.is_positive()) {
            [self scheduleNextWatchForButtonsWithTimeout:timeout -
                                                         kWatcherCycleDelay
                                               runNumber:runNumber];
          } else {
            [watchingButtons removeAllObjects];
          }
        });
      });
}

// Looks for a button (based on traits) with the given `label`,
// recursively in the given `views`.
+ (void)findButtonsWithLabelsInViews:(NSArray<UIView*>*)views {
  if (!watchingButtons.count)
    return;

  for (UIView* view in views) {
    [self buttonsWithLabelsMatchView:view];
    [self findButtonsWithLabelsInViews:view.subviews];
  }
}

// Checks if the given `view` is a button (based on traits) with the
// given accessibility label.
+ (void)buttonsWithLabelsMatchView:(UIView*)view {
  if (![view respondsToSelector:@selector(accessibilityLabel)])
    return;
  if (([view accessibilityTraits] & UIAccessibilityTraitButton) == 0)
    return;
  if ([watchingButtons containsObject:view.accessibilityLabel]) {
    [watchedButtons addObject:view.accessibilityLabel];
    [watchingButtons removeObject:view.accessibilityLabel];
  }
}

#pragma mark - Default Browser Promo Utilities

+ (void)clearDefaultBrowserPromoData {
  ClearDefaultBrowserPromoData();
}

+ (void)copyURLToPasteBoard {
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  pasteboard.URL = [NSURL URLWithString:@"chrome://version"];
}

+ (void)disableDefaultBrowserPromo {
  chrome_test_util::GetMainController().appState.shouldShowDefaultBrowserPromo =
      NO;
  LogUserInteractionWithFullscreenPromo();
}

@end
