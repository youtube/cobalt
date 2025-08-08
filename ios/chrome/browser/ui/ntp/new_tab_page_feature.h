// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

#pragma mark - Feature declarations

// Feature flag to enable showing a live preview for Discover feed when opening
// the feed context menu.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedPreview);

// Feature flag to enable static resource serving for the Discover feed.
// TODO(crbug.com/1385512): Remove this.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedStaticResourceServing);

// Feature flag to enable discofeed endpoint for the Discover feed.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedDiscoFeedEndpoint);

// Feature flag to enable the sync promo on top of the discover feed.
BASE_DECLARE_FEATURE(kEnableDiscoverFeedTopSyncPromo);

// Feature flag to fix the NTP view hierarchy if it is broken before applying
// constraints.
// TODO(crbug.com/1262536): Remove this when it is fixed.
BASE_DECLARE_FEATURE(kEnableNTPViewHierarchyRepair);

// Flag to modify the feed header through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kFeedHeaderSettings);

// Flag to override feed settings through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_DECLARE_FEATURE(kOverrideFeedSettings);

// Feature flag to enable synthentic capabilities.
BASE_DECLARE_FEATURE(kEnableFeedSyntheticCapabilities);

// Feature flag to enable sending discover feedback to an updated target
BASE_DECLARE_FEATURE(kWebFeedFeedbackReroute);

// Feature flag to enable follow management page instant reload when being
// opened.
BASE_DECLARE_FEATURE(kEnableFollowManagementInstantReload);

// Feature flag to enable signed out user view demotion.
BASE_DECLARE_FEATURE(kEnableSignedOutViewDemotion);

#pragma mark - Feature parameters

// A parameter to indicate whether Reconstructed Templates is enabled for static
// resource serving.
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverFeedSRSReconstructedTemplatesEnabled[];

// A parameter to indicate whether Preload Templates is enabled for static
// resource serving.
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverFeedSRSPreloadTemplatesEnabled[];

// Parameter for the feed top sync promo's style.
extern const char kDiscoverFeedTopSyncPromoStyle[];

// Feature parameters for the feed header settings.
extern const char kDisableStickyHeaderForFollowingFeed[];
extern const char kOverrideFeedHeaderHeight[];

// A parameter value for the feed's refresh threshold when the feed has already
// been seen by the user.
extern const char kFeedSettingRefreshThresholdInSeconds[];

// A parameter value for the feed's refresh threshold when the feed has not been
// seen by the user.
extern const char kFeedSettingUnseenRefreshThresholdInSeconds[];

// A parameter value for the feed's maximum data cache age.
extern const char kFeedSettingMaximumDataCacheAgeInSeconds[];

// A parameter value for the timeout threshold after clearing browsing data.
extern const char kFeedSettingTimeoutThresholdAfterClearBrowsingData[];

// A parameter value for the feed referrer.
extern const char kFeedSettingDiscoverReferrerParameter[];

#pragma mark - Helpers

// Whether the Discover feed content preview is shown in the context menu.
bool IsDiscoverFeedPreviewEnabled();

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

// Whether the sync promo should be shown on top of the feed.
bool IsDiscoverFeedTopSyncPromoEnabled();

// Returns the feed top sync promo's UI style.
SigninPromoViewStyle GetTopOfFeedPromoStyle();

// Whether the feed top sync promo should only be shown to users who previously
// engaged with the feed.
bool ShouldIgnoreFeedEngagementConditionForTopSyncPromo();

// Returns the number of impressions before autodismissing the feed sync promo.
int FeedSyncPromoAutodismissCount();

// Whether content suggestions are enabled for supervised users.
bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service);

// YES if the Following feed header should not be sticky.
bool IsStickyHeaderDisabledForFollowingFeed();

// YES if a dot should appear to indicate that there is new content in the
// Following feed.
bool IsDotEnabledForNewFollowedContent();

// YES if synthetic capabilities will be used to inform the server of client
// capabilities.
bool IsFeedSyntheticCapabilitiesEnabled();

// Returns a custom height for the Following feed header if it is overridden
// from the server, or returns the default value.
int FollowingFeedHeaderHeight();

// YES if discover feedback is going to be sent to the updated target.
bool IsWebFeedFeedbackRerouteEnabled();

// Yes when enabling follow management page instant reload when being opened.
bool IsFollowManagementInstantReloadEnabled();

// Yes if the signed out user view demotion is enabled.
bool IsSignedOutViewDemotionEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
