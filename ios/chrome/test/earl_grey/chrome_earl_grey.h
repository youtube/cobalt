// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_

#import <UIKit/UIKit.h>

#include <string>

#include "base/time/time.h"
#import "components/content_settings/core/common/content_settings.h"
#include "components/sync/base/model_type.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "url/gurl.h"

@class FakeSystemIdentity;
@class ElementSelector;
@protocol GREYAction;
@protocol GREYMatcher;

namespace chrome_test_util {

// Returns current keyWindow, from the list of all of the remote application
// windows. Use only for single window tests.
UIWindow* GetAnyKeyWindow();

}  // namespace chrome_test_util

// Overload of grey_longPressWithDuration() taking a base::TimeDelta.
id<GREYAction> grey_longPressWithDuration(base::TimeDelta duration);

#define ChromeEarlGrey \
  [ChromeEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Test methods that perform actions on Chrome. These methods may read or alter
// Chrome's internal state programmatically or via the UI, but in both cases
// will properly synchronize the UI for Earl Grey tests.
@interface ChromeEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - Test Utilities

// Wait until `matcher` is accessible (not nil) on the device.
- (void)waitForMatcher:(id<GREYMatcher>)matcher;

#pragma mark - Device Utilities

// Returns YES if running on an iPad.
- (BOOL)isIPadIdiom;

// YES if the current interface language uses RTL layout.
- (BOOL)isRTL;

// Returns YES if the main application window's rootViewController has a compact
// horizontal size class.
- (BOOL)isCompactWidth;

// Returns YES if the main application window's rootViewController has a compact
// vertical size class.
- (BOOL)isCompactHeight;

// Returns whether the toolbar is split between top and bottom toolbar or if it
// is displayed as only one toolbar.
- (BOOL)isSplitToolbarMode;

// Whether the the main application window's rootViewController has a regular
// vertical and regular horizontal size class.
- (BOOL)isRegularXRegularSizeClass;

#pragma mark - History Utilities (EG2)

// Clears browsing history. Raises an EarlGrey exception if history is not
// cleared within a timeout.
- (void)clearBrowsingHistory;

// Gets the number of entries in the browsing history database. GREYAssert is
// induced on error.
- (NSInteger)browsingHistoryEntryCount;

// Gets the number of items in the back list.
- (NSInteger)navigationBackListItemsCount;

// Clears browsing cache. Raises an EarlGrey exception if history is not
// cleared within a timeout.
- (void)removeBrowsingCache;

// Persists the current list of tabs to disk immediately.
- (void)saveSessionImmediately;

#pragma mark - Navigation Utilities (EG2)

// Instructs some connected scene to open `URL` with default opening
// options.
- (void)sceneOpenURL:(const GURL&)URL;

// Loads `URL` in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and if waitForCompletion is YES
// waits for the loading to complete within a timeout.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
- (void)loadURL:(const GURL&)URL waitForCompletion:(BOOL)wait;

// Loads `URL` in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and waits for the loading to complete within a
// timeout.
// If the condition is not met within a timeout returns an NSError indicating
// why the operation failed, otherwise nil.
- (void)loadURL:(const GURL&)URL;

// Returns YES if the current WebState is loading.
- (BOOL)isLoading [[nodiscard]];

// Reloads the page and waits for the loading to complete within a timeout, or a
// GREYAssert is induced.
- (void)reload;

// Reloads the page. If `wait` is YES, waits for the loading to complete within
// a timeout, or a GREYAssert is induced.
- (void)reloadAndWaitForCompletion:(BOOL)wait;

// Navigates back to the previous page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
- (void)goBack;

// Navigates forward to the next page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
- (void)goForward;

// Waits for the page to finish loading within a timeout, or a GREYAssert is
// induced.
- (void)waitForPageToFinishLoading;

// Waits for the matcher to return an element that is sufficiently visible.
- (void)waitForSufficientlyVisibleElementWithMatcher:(id<GREYMatcher>)matcher;

// Waits for the matcher to return an element that is not sufficiently visible
// (or nil).
- (void)waitForNotSufficientlyVisibleElementWithMatcher:
    (id<GREYMatcher>)matcher;

// Waits for the matcher to return an element.
- (void)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher;

// Waits for the matcher to return an element. Returns whether the element did
// appear in the delay.
- (BOOL)testUIElementAppearanceWithMatcher:(id<GREYMatcher>)matcher;

// Waits for the matcher to return an element. If the condition is not met
// within the given `timeout` a GREYAssert is induced.
- (void)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher
                                    timeout:(base::TimeDelta)timeout;

// Waits for the matcher to return an element. Returns wheher the condition is
// met within the given `timeout`.
- (BOOL)testUIElementAppearanceWithMatcher:(id<GREYMatcher>)matcher
                                   timeout:(base::TimeDelta)timeout;

// Waits for the matcher to not return any elements.
- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher;

// Waits for the matcher to not return any elements. If the condition is not met
// within the given `timeout` a GREYAssert is induced.
- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher
                                       timeout:(base::TimeDelta)timeout;

// Waits for there to be `count` number of non-incognito tabs within a timeout,
// or a GREYAssert is induced.
- (void)waitForMainTabCount:(NSUInteger)count;

// Waits for there to be `count` number of incognito tabs within a timeout, or a
// GREYAssert is induced.
- (void)waitForIncognitoTabCount:(NSUInteger)count;

// Loads `URL` as if it was opened from an external application.
- (void)openURLFromExternalApp:(const GURL&)URL;

// Programmatically dismisses settings screen.
- (void)dismissSettings;

#pragma mark - Sync Utilities (EG2)

// Clears fake sync server data if the server is running.
- (void)clearSyncServerData;

// Signs in with `identity` without sync consent.
- (void)signInWithoutSyncWithIdentity:(FakeSystemIdentity*)identity;

// Injects user demographics into the fake sync server. `rawBirthYear` is the
// true birth year, pre-noise, and the gender corresponds to the proto enum
// UserDemographicsProto::Gender.
- (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender;

// Clears the autofill profile for the given `GUID`.
- (void)clearAutofillProfileWithGUID:(const std::string&)GUID;

// Injects an autofill profile into the fake sync server with `GUID` and
// `full_name`.
- (void)addAutofillProfileToFakeSyncServerWithGUID:(const std::string&)GUID
                               autofillProfileName:(const std::string&)fullName;

// Returns YES if there is an autofilll profile with the corresponding `GUID`
// and `full_name`.
- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)GUID
                     autofillProfileName:(const std::string&)fullName
    [[nodiscard]];

// Sets up a fake sync server to be used by the SyncServiceImpl.
- (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the SyncServiceImpl and restores the
// real one.
- (void)tearDownFakeSyncServer;

// Gets the number of entities of the given `type`.
- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type [[nodiscard]];

// Adds typed URL into HistoryService.
- (void)addHistoryServiceTypedURL:(const GURL&)URL;

// Deletes typed URL from HistoryService.
- (void)deleteHistoryServiceTypedURL:(const GURL&)URL;

// Injects a bookmark with `URL` and `title` into the fake sync server.
- (void)addFakeSyncServerBookmarkWithURL:(const GURL&)URL
                                   title:(const std::string&)title;

// Injects a legacy bookmark into the fake sync server. The legacy bookmark
// means 2015 and earlier, prior to the adoption of GUIDs for originator client
// item ID.
- (void)addFakeSyncServerLegacyBookmarkWithURL:(const GURL&)URL
                                         title:(const std::string&)title
                     originator_client_item_id:
                         (const std::string&)originator_client_item_id;

// Injects a typed URL to the sync FakeServer.
- (void)addFakeSyncServerTypedURL:(const GURL&)URL;

// Injects a HISTORY visit to the sync FakeServer.
- (void)addFakeSyncServerHistoryVisit:(const GURL&)URL;

// Injects device info to sync FakeServer.
- (void)addFakeSyncServerDeviceInfo:(NSString*)deviceName
               lastUpdatedTimestamp:(base::Time)lastUpdatedTimestamp;

// Triggers a sync cycle for a `type`.
- (void)triggerSyncCycleForType:(syncer::ModelType)type;

// Deletes an autofill profile from the fake sync server with `GUID`, if it
// exists. If it doesn't exist, nothing is done.
- (void)deleteAutofillProfileFromFakeSyncServerWithGUID:
    (const std::string&)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. `URLs` is
// the collection of URLs that are to be expected for a single window. A
// GREYAssert is induced on failure. See the SessionsHierarchy class for
// documentation regarding the verification.
- (void)verifySyncServerSessionURLs:(NSArray<NSString*>*)URLs;

// Waits until sync server contains `count` entities of the given `type` and
// `name`. Folders are not included in this count.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                     name:(const std::string&)UTF8Name
                                    count:(size_t)count
                                  timeout:(base::TimeDelta)timeout;

- (void)waitForSyncServerHistoryURLs:(NSArray<NSURL*>*)URLs
                             timeout:(base::TimeDelta)timeout;

// Induces a GREYAssert if `expectPresent` is YES and the provided `URL` is
// not present in the history DB, or vice versa.
- (void)waitForHistoryURL:(const GURL&)URL
            expectPresent:(BOOL)expectPresent
                  timeout:(base::TimeDelta)timeout;

// Waits for sync invalidation field presence in the DeviceInfo data type on the
// server.
- (void)waitForSyncInvalidationFields;

#pragma mark - Tab Utilities (EG2)

// Opens a new tab and waits for the new tab animation to complete within a
// timeout, or a GREYAssert is induced.
- (void)openNewTab;

// Simulates opening `url` from another application.
- (void)simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:(GURL)url;

// Simulates opening the add account sign-in flow from the web.
- (void)simulateAddAccountFromWeb;

// Closes the current tab and waits for the UI to complete.
- (void)closeCurrentTab;

// Opens a new incognito tab and waits for the new tab animation to complete.
- (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and waits for the
// UI to complete. If current mode is Incognito, mode will be switched to
// normal after closing all tabs.
- (void)closeAllTabsInCurrentMode;

// Closes all normal (non-incognito) tabs and waits for the UI to complete
// within a timeout, or a GREYAssert is induced.
- (void)closeAllNormalTabs;

// Closes all incognito tabs and waits for the UI to complete within a
// timeout, or a GREYAssert is induced.
- (void)closeAllIncognitoTabs;

// Closes all tabs in the all modes (incognito and main (non-incognito)), and
// does not wait for the UI to complete. If current mode is Incognito, mode will
// be switched to main (non-incognito) after closing the incognito tabs.
- (void)closeAllTabs;

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
- (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
- (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
- (BOOL)isIncognitoMode [[nodiscard]];

// Returns the number of main (non-incognito) tabs.
- (NSUInteger)mainTabCount [[nodiscard]];

// Returns the number of incognito tabs.
- (NSUInteger)incognitoTabCount [[nodiscard]];

// Returns the number of browsers.
- (NSUInteger)browserCount [[nodiscard]];

// Returns the index of active tab in normal (non-incognito) mode.
- (NSUInteger)indexOfActiveNormalTab;

// Simulates a backgrounding and raises an EarlGrey exception if simulation not
// succeeded.
- (void)simulateTabsBackgrounding;

// Returns the number of main (non-incognito) tabs currently evicted.
- (NSUInteger)evictedMainTabCount [[nodiscard]];

// Evicts the tabs associated with the non-current browser mode.
- (void)evictOtherBrowserTabs;

// Sets the normal tabs as 'cold start' tabs and raises an EarlGrey exception if
// operation not succeeded.
- (void)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode and raises an EarlGrey
// exception if operation not succeeded.
- (void)resetTabUsageRecorder;

// Returns the tab title of the current tab.
- (NSString*)currentTabTitle;

// Returns the tab title of the next tab. Assumes that next tab exists.
- (NSString*)nextTabTitle;

// Returns a unique identifier for the current Tab.
- (NSString*)currentTabID;

// Returns a unique identifier for the next Tab.
- (NSString*)nextTabID;

// Shows the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
- (void)showTabSwitcher;

#pragma mark - Window utilities (EG2)

// Returns screen position of the given `windowNumber`
- (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber;

// Returns the number of windows, including background and disconnected or
// archived windows.
- (NSUInteger)windowCount [[nodiscard]];

// Returns the number of foreground (visible on screen) windows.
- (NSUInteger)foregroundWindowCount [[nodiscard]];

// Waits for there to be `count` number of browsers within a timeout,
// or a GREYAssert is induced.
- (void)waitForForegroundWindowCount:(NSUInteger)count;

// Closes all but one window, including all non-foreground windows. No-op if
// only one window presents.
- (void)closeAllExtraWindows;

// Opens a new window.
- (void)openNewWindow;

// After opening a new window (through openNewWindow or otherwise) a call
// to this should be made to make sure the new window is ready to interact
// or to be closed. Closing while setting up leads to crashes.
- (void)waitUntilReadyWindowWithNumber:(int)windowNumber;

// Opens a new tab in window with given number and waits for the new tab
// animation to complete within a timeout, or a GREYAssert is induced.
- (void)openNewTabInWindowWithNumber:(int)windowNumber;

// Closes the window with given number. Note that numbering doesn't change and
// if a new window is to be added in a test, a renumbering might be needed.
- (void)closeWindowWithNumber:(int)windowNumber;

// Renumbers given window with current number to new number. For example, if
// you have windows 0 and 1, close window 0, the add new window, then both
// windows would be 1. Therefore you should renumber the remaining ones
// before adding new ones.
- (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber;

// Loads `URL` in the current WebState for window with given number, with
// transition type ui::PAGE_TRANSITION_TYPED, and if waitForCompletion is YES
// waits for the loading to complete within a timeout.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
- (void)loadURL:(const GURL&)URL
    inWindowWithNumber:(int)windowNumber
     waitForCompletion:(BOOL)wait;

// Loads `URL` in the current WebState for window with given number, with
// transition type ui::PAGE_TRANSITION_TYPED, and waits for the loading to
// complete within a timeout. If the condition is not met within a timeout
// returns an NSError indicating why the operation failed, otherwise nil.
- (void)loadURL:(const GURL&)URL inWindowWithNumber:(int)windowNumber;

// Waits for the page to finish loading for window with given number, within a
// timeout, or a GREYAssert is induced.
- (void)waitForPageToFinishLoadingInWindowWithNumber:(int)windowNumber;

// Returns YES if the window with given number's current WebState is loading.
- (BOOL)isLoadingInWindowWithNumber:(int)windowNumber [[nodiscard]];

// Waits for the current web state for window to be visible.
- (void)waitForWebStateVisible;

// Waits for the current web state for window with given number, to contain
// `UTF8Text`. If the condition is not met within a timeout a GREYAssert is
// induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                   inWindowWithNumber:(int)windowNumber;

// Waits for the current web state for window with given number, to contain
// `UTF8Text`. If the condition is not met within the given `timeout` a
// GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(base::TimeDelta)timeout
                   inWindowWithNumber:(int)windowNumber;

// Waits for there to be `count` number of non-incognito tabs within a timeout,
// or a GREYAssert is induced.
- (void)waitForMainTabCount:(NSUInteger)count
         inWindowWithNumber:(int)windowNumber;

// Waits for there to be `count` number of incognito tabs within a timeout, or a
// GREYAssert is induced.
- (void)waitForIncognitoTabCount:(NSUInteger)count
              inWindowWithNumber:(int)windowNumber;

// Waits for the JavaScript query `javaScriptCondition` to return `boolValue`
// YES. If the condition is not met within kWaitForActionTimeout a GREYAssert is
// induced.
- (void)waitForJavaScriptCondition:(NSString*)javaScriptCondition;

#pragma mark - SignIn Utilities (EG2)

// Signs the user out, clears the known accounts entirely and checks whether the
// accounts were correctly removed from the keychain. Induces a GREYAssert if
// the operation fails.
- (void)signOutAndClearIdentities;

#pragma mark - Sync Utilities (EG2)

// Waits for sync engine to be initialized or not. It doesn't necessarily mean
// that data types are configured and ready to use. See
// SyncService::IsEngineInitialized() for details. If not succeeded a GREYAssert
// is induced.
- (void)waitForSyncEngineInitialized:(BOOL)isInitialized
                         syncTimeout:(base::TimeDelta)timeout;

// Waits for the sync feature to be enabled/disabled. See SyncService::
// IsSyncFeatureEnabled() for details. If not succeeded a GREYAssert is induced.
- (void)waitForSyncFeatureEnabled:(BOOL)isEnabled
                      syncTimeout:(base::TimeDelta)timeout;

// Waits for sync to become fully active; see
// SyncService::TransportState::ACTIVE for details. If not succeeded a
// GREYAssert is induced.
- (void)waitForSyncTransportStateActiveWithTimeout:(base::TimeDelta)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
- (std::string)syncCacheGUID;

// Adds a bookmark with a sync passphrase. The sync server will need the sync
// passphrase to start.
- (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase;

#pragma mark - WebState Utilities (EG2)

// Taps html element with `elementID` in the current web state.
// A GREYAssert is induced on failure.
- (void)tapWebStateElementWithID:(NSString*)elementID;

// Attempts to tap the element with `element_id` within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes.
// A GREYAssert is induced on failure.
- (void)tapWebStateElementInIFrameWithID:(const std::string&)elementID;

// Waits for the current web state to contain an element matching `selector`.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingElement:(ElementSelector*)selector;

// Waits for the current web state to NOT contain an element matching
// `selector`. If the condition is not met within a timeout a GREYAssert is
// induced.
- (void)waitForWebStateNotContainingElement:(ElementSelector*)selector;

// Attempts to submit form with `formID` in the current WebState.
// Induces a GREYAssert if the operation fails.
- (void)submitWebStateFormWithID:(const std::string&)formID;

// Waits for the current web state to contain `UTF8Text`. If the condition is
// not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text;

// Waits for the main frame or an iframe to contain `UTF8Text`. If the condition
// is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateFrameContainingText:(const std::string&)UTF8Text;

// Waits for the current web state to contain `UTF8Text`. If the condition is
// not met within the given `timeout` a GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(base::TimeDelta)timeout;

// Waits for there to be no web state containing `UTF8Text`.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateNotContainingText:(const std::string&)UTF8Text;

// Waits for there to be a web state containing a blocked `imageID`.  When
// blocked, the image element will be smaller than the actual image size.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingBlockedImageElementWithID:
    (const std::string&)UTF8ImageID;

// Waits for there to be a web state containing loaded image with `imageID`.
// When loaded, the image element will have the same size as actual image.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForWebStateContainingLoadedImageElementWithID:
    (const std::string&)UTF8ImageID;

// Waits for the web state's scroll view zoom scale to be suitably close (within
// 0.05) of the expected scale. Returns nil if the condition is met within a
// timeout, or else an NSError indicating why the operation failed.
- (void)waitForWebStateZoomScale:(CGFloat)scale;

// Returns the current web state's VisibleURL.
- (GURL)webStateVisibleURL;

// Returns the current web state's last committed URL.
- (GURL)webStateLastCommittedURL;

// Purges cached web view pages, so the next time back navigation will not use
// a cached page. Browsers don't have to use a fresh version for back/forward
// navigation for HTTP pages and may serve a version from the cache even if the
// Cache-Control response header says otherwise.
- (void)purgeCachedWebViewPages;

// Simulators background, killing, and restoring the app within the limitations
// of EG1, by simply doing a tab grid close all / undo / done.
- (void)triggerRestoreViaTabGridRemoveAllUndo;

// Returns YES if the current WebState's web view uses the content inset to
// correctly align the top of the content with the bottom of the top bar.
- (BOOL)webStateWebViewUsesContentInset;

// Returns the size of the current WebState's web view.
- (CGSize)webStateWebViewSize;

// Stops any pending navigations in all WebStates which are loading.
- (void)stopAllWebStatesLoading;

// Clears all web state browsing data then relaunches the application. (A
// GREYAssert is induced if the data cannot be cleared.)
// NOTE: The passed in `config` will be modified to set `relaunch_policy` to
// `ForceRelaunchByKilling.
- (void)clearAllWebStateBrowsingData:(AppLaunchConfiguration)config;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If the condition is not met within a timeout a GREYAssert is induced.
- (void)waitForBookmarksToFinishLoading;

// Clears bookmarks if any bookmark still presents. A GREYAssert is induced if
// bookmarks can not be cleared.
- (void)clearBookmarks;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with `URL` if that page
// doesn't specify a title.
- (NSString*)displayTitleForURL:(const GURL&)URL;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout a GREYAssert is
// induced.

- (base::Value)evaluateJavaScript:(NSString*)javaScript [[nodiscard]];

// Executes JavaScript on current WebState. This function should be used in
// place -evaluateJavaScript when the executed JavaScript's return value will
// not be used.
- (void)evaluateJavaScriptForSideEffect:(NSString*)javaScript;

// Returns the user agent that should be used for the mobile version.
- (NSString*)mobileUserAgentString;

#pragma mark - Cookie Utilities (EG2)

// Returns cookies as key value pairs, where key is a cookie name and value is a
// cookie value.
// A GREYAssert is induced if cookies can not be returned.
- (NSDictionary*)cookies;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that all interactive elements on screen (or at least one of their
// descendants) are accessible.
// A GREYAssert is induced if not all elements are accessible.
- (void)verifyAccessibilityForCurrentScreen;

#pragma mark - Feature enables checkers (EG2)

// Returns YES if `variationID` is enabled.
- (BOOL)isVariationEnabled:(int)variationID;

// Returns YES if a variation triggering server-side behavior is enabled.
- (BOOL)isTriggerVariationEnabled:(int)variationID;

// Returns YES if UKM feature is enabled.
- (BOOL)isUKMEnabled [[nodiscard]];

// Returns YES if kSynthesizedRestoreSessionEnabled feature is enabled.
- (BOOL)isSynthesizedRestoreSessionEnabled [[nodiscard]];

// Returns YES if kTestFeature is enabled.
- (BOOL)isTestFeatureEnabled;

// Returns YES if DemographicMetricsReporting feature is enabled.
- (BOOL)isDemographicMetricsReportingEnabled [[nodiscard]];

// Returns YES if the SyncEnableHistoryDataType feature is enabled.
- (BOOL)isSyncHistoryDataTypeEnabled [[nodiscard]];

// Returns YES if the `launchSwitch` is found in host app launch switches.
- (BOOL)appHasLaunchSwitch:(const std::string&)launchSwitch;

// Returns YES if custom WebKit frameworks were properly loaded, rather than
// system frameworks. Always returns YES if the app was not requested to run
// with custom WebKit frameworks.
- (BOOL)isCustomWebKitLoadedIfRequested [[nodiscard]];

// Returns YES if error pages are displayed using loadSimulatedRequest.
- (BOOL)isLoadSimulatedRequestAPIEnabled;

// Returns whether the mobile version of the websites are requested by default.
- (BOOL)isMobileModeByDefault [[nodiscard]];

// Returns whether the app is configured to, and running in an environment which
// can, open multiple windows.
- (BOOL)areMultipleWindowsSupported;

// Returns whether the NewOverflowMenu feature is enabled.
- (BOOL)isNewOverflowMenuEnabled;

// Returns whether the UseLensToSearchForImage feature is enabled;
- (BOOL)isUseLensToSearchForImageEnabled;

// Returns whether the Thumbstrip feature is enabled for window with given
// number.
- (BOOL)isThumbstripEnabledForWindowWithNumber:(int)windowNumber;

// Returns whether the Web Channels feature is enabled.
- (BOOL)isWebChannelsEnabled;

// Returns whether UIButtonConfiguration changes are enabled.
- (BOOL)isUIButtonConfigurationEnabled;

#pragma mark - ContentSettings

// Gets the current value of the popup content setting preference for the
// original browser state.
- (ContentSetting)popupPrefValue;

// Sets the popup content setting preference to the given value for the original
// browser state.
- (void)setPopupPrefValue:(ContentSetting)value;

// Resets the desktop content setting to its default value.
- (void)resetDesktopContentSetting;

#pragma mark - Keyboard utilities

// The count of key commands registered with the currently active BVC.
- (NSInteger)registeredKeyCommandCount;

// Simulates a physical keyboard event.
// The input is similar to UIKeyCommand parameters, and is designed for testing
// keyboard shortcuts.
// Accepts any strings and also UIKeyInput{Up|Down|Left|Right}Arrow and
// UIKeyInputEscape constants as `input`.
- (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags;

#pragma mark - Default Utilities (EG2)

// Stores a value for the provided key in NSUserDefaults.
- (void)setUserDefaultObject:(id)value forKey:(NSString*)defaultName;

// Removes the object for `key` in NSUserDefault.
- (void)removeUserDefaultObjectForKey:(NSString*)key;

#pragma mark - Pref Utilities (EG2)

// Gets the value of a local state pref.
- (bool)localStateBooleanPref:(const std::string&)prefName;
- (int)localStateIntegerPref:(const std::string&)prefName;
- (std::string)localStateStringPref:(const std::string&)prefName;

// Sets the integer values for the local state pref with `prefName`. `value`
// can be either a casted enum or any other numerical value. Local State
// contains the preferences that are shared between all browser states.
- (void)setIntegerValue:(int)value
      forLocalStatePref:(const std::string&)prefName;

// Gets the value of a user pref in the original browser state.
- (bool)userBooleanPref:(const std::string&)prefName;
- (int)userIntegerPref:(const std::string&)prefName;
- (std::string)userStringPref:(const std::string&)prefName;

// Sets the value of a user pref in the original browser state.
- (void)setBoolValue:(BOOL)value forUserPref:(const std::string&)UTF8PrefName;
- (void)setIntegerValue:(int)value forUserPref:(const std::string&)UTF8PrefName;

// Resets the BrowsingDataPrefs, which defines if its selected or not when
// clearing Browsing data.
- (void)resetBrowsingDataPrefs;

// Resets data for the local state pref with `prefName`.
- (void)resetDataForLocalStatePref:(const std::string&)prefName;

#pragma mark - Pasteboard Utilities (EG2)

// Verifies that `text` was copied to the pasteboard.
- (void)verifyStringCopied:(NSString*)text;

// Returns YES if general pasteboard images property contains a nonempty array.
- (BOOL)pasteboardHasImages;

// Retrieves the GURL stored in the Pasteboard. Returns an empty GURL if no
// URL is currently in the pasteboard.
- (GURL)pasteboardURL;

// Clears the pasteboard.
- (void)clearPasteboard;

// Copies `text` into the clipboard from the app's perspective.
- (void)copyTextToPasteboard:(NSString*)text;

#pragma mark - Context Menus Utilities (EG2)

// Taps on the Copy Link context menu action and verifies that the `text` has
// been copied to the pasteboard.
- (void)verifyCopyLinkActionWithText:(NSString*)text;

// Taps on the Open in New Tab context menu action and waits for the `URL` to be
// present in the omnibox.
- (void)verifyOpenInNewTabActionWithURL:(const std::string&)URL;

// Taps on the Open in New Window context menu action and waits for the
// `content` to be present in webview.
- (void)verifyOpenInNewWindowActionWithContent:(const std::string&)content;

// Taps on the Open in Incognito context menu action and waits for the `URL` to
// be present in the omnibox.
- (void)verifyOpenInIncognitoActionWithURL:(const std::string&)URL;

// Taps on the Share context menu action and validates that the ActivityView
// was brought up with the correct title in its header. The title starts as the
// host of the loaded `URL` and is then updated to the page title `pageTitle`.
- (void)verifyShareActionWithURL:(const GURL&)URL
                       pageTitle:(NSString*)pageTitle;

#pragma mark - Unified Consent utilities

// Enables or disables URL-keyed anonymized data collection.
- (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled;

#pragma mark - Watcher utilities

// Starts monitoring for buttons (based on traits) with the given
// (accessibility) `labels`. Monitoring will stop once all are found, or if
// timeout expires. If a previous set is currently being watched for it gets
// replaced with this set. Note that timeout is best effort and can be a bit
// longer than specified. This method returns immediately.
- (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(base::TimeDelta)timeout;

// Returns YES if the button with given (accessibility) `label` was observed at
// some point since `watchForButtonsWithLabels:timeout:` was called.
- (BOOL)watcherDetectedButtonWithLabel:(NSString*)label;

// Clear the watcher list, stopping monitoring.
- (void)stopWatcher;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
