// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

namespace commerce {
class ShoppingService;
}

namespace favicon {
class LargeIconService;
}

namespace ntp_tiles {
class MostVisitedSites;
}

namespace segmentation_platform {
class SegmentationPlatformService;
}

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

@protocol ApplicationCommands;
class AuthenticationService;
class Browser;
@protocol BrowserCoordinatorCommands;
@protocol ContentSuggestionsDelegate;
@class ContentSuggestionsMetricsRecorder;
enum class ContentSuggestionsModuleType;
class GURL;
class LargeIconCache;
@protocol NewTabPageMetricsDelegate;
@class ParcelTrackingItem;
enum class ParcelType;
class PromosManager;
class ReadingListModel;
@protocol SnackbarCommands;
class WebStateList;

// Mediator for ContentSuggestions.
@interface ContentSuggestionsMediator
    : NSObject <ContentSuggestionsCommands,
                ContentSuggestionsGestureCommands,
                StartSurfaceRecentTabObserving>

// Default initializer.
- (instancetype)
         initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                 readingListModel:(ReadingListModel*)readingListModel
                      prefService:(PrefService*)prefService
    isGoogleDefaultSearchProvider:(BOOL)isGoogleDefaultSearchProvider
                      syncService:(syncer::SyncService*)syncService
            authenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
                  shoppingService:(commerce::ShoppingService*)shoppingService
                          browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCoordinatorCommands, SnackbarCommands>
        dispatcher;

// Command handler for the mediator.
@property(nonatomic, weak)
    id<ContentSuggestionsCommands, ContentSuggestionsGestureCommands>
        commandHandler;

// Delegate used to communicate Content Suggestions events to the delegate.
@property(nonatomic, weak) id<ContentSuggestionsDelegate> delegate;

// The consumer that will be notified when the data change.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// WebStateList associated with this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;

// The promos manager to alert if the user uses What's New.
@property(nonatomic, assign) PromosManager* promosManager;

// Delegate for reporting content suggestions actions to the NTP metrics
// recorder.
@property(nonatomic, weak) id<NewTabPageMetricsDelegate> NTPMetricsDelegate;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// TODO(crbug.com/1462664): Move to initializer param once
// kSegmentationPlatformIosModuleRanker is launched. Segmentation Platform
// Service.
@property(nonatomic, assign)
    segmentation_platform::SegmentationPlatformService* segmentationService;

// Disconnects the mediator.
- (void)disconnect;

// Trigger a refresh of the Content Suggestions Most Visited tiles.
- (void)refreshMostVisitedTiles;

// Block `URL` from Most Visited sites.
- (void)blockMostVisitedURL:(GURL)URL;

// Always allow `URL` in Most Visited sites.
- (void)allowMostVisitedURL:(GURL)URL;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

// Whether the most recent tab tile is being shown. Returns YES if
// configureMostRecentTabItemWithWebState: has been called.
- (BOOL)mostRecentTabStartSurfaceTileIsShowing;

// Configures the most recent tab item with `webState` and `timeLabel`.
- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel;

// Indicates that the "Return to Recent Tab" tile should be hidden.
- (void)hideRecentTabTile;

// Disables and hide the Set Up List.
- (void)disableSetUpList;

// Disables the tab resumption tile.
- (void)disableTabResumption;

// Disables and hides the Safety Check module, `type`, in the Magic Stack.
- (void)disableSafetyCheck:(ContentSuggestionsModuleType)type;

// Disables and hides the parcel tracking module.
- (void)disableParcelTracking;

// Indicates that `parcelID` should be untracked.
- (void)untrackParcel:(NSString*)parcelID;

// Indicates that `parcelID` should be tracked.
- (void)trackParcel:(NSString*)parcelID carrier:(ParcelType)carrier;

// Returns all possible items in the Set Up List.
- (NSArray<SetUpListItemViewData*>*)allSetUpListItems;

// Returns the latest fetched tracked parcels.
- (NSArray<ParcelTrackingItem*>*)parcelTrackingItems;

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
