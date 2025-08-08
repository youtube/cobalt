// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_FEATURES_H_
#define COMPONENTS_LENS_LENS_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

// Enables context menu search by image sending to the Lens homepage.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensStandalone);

// Feature that controls the compression of images before they are sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensImageCompression);

// Enables a variety of changes aimed to improve user's engagement with current
// Lens features.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchOptimizations);

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableLatencyLogging);

// Enable keyboard shortcut for the Lens Region Search feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableRegionSearchKeyboardShortcut);

// Enables context menu option for translating image feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableImageTranslate);

// Enables the image search side panel experience for third party default search
// engines
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableImageSearchSidePanelFor3PDse);

// Enables launching the region search experience in a new tab with WebUI.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensRegionSearchStaticPage);

// Enables using more optimized image formats for Lens requests.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensImageFormatOptimizations);

// Enables the context menu in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(EnableContextMenuInLensSidePanel);

// Enables the ping for Lens Standalone, which is sent before the Standalone
// request.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableLensPing);

// The base URL for Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<std::string> kHomepageURLForLens;

// Enable Lens HTML redirect fix.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableLensHtmlRedirectFix;

// Enable Lens loading state removal on
// DocumentOnLoadCompletedInPrimaryMainFrame.
// TODO(crbug/1452161): Clean up unused listeners and flags after determining
// which ones we want to listen to for server-side rendering backends.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool>
    kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame;

// Enable Lens loading state removal on DomContentLoaded.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDomContentLoaded;

// Enable Lens loading state removal on DidFinishNavigation.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDidFinishNavigation;

// Enable Lens loading state removal on NavigationEntryCommitted.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool>
    kDismissLoadingStateOnNavigationEntryCommitted;

// Enable Lens loading state removal on DidFinishLoad.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDidFinishLoad;

// Enable Lens loading state removal on PrimaryPageChanged.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnPrimaryPageChanged;

// Enables Lens fullscreen search on Desktop platforms.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableFullscreenSearch;

// Enables encoding to WebP for region search queries. This param takes
// precedence over kUseJpegInRegionSearch.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseWebpInRegionSearch;

// Enables encoding to JPEG for region search queries. This param does
// nothing if kUseWebpInRegionSearch is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseJpegInRegionSearch;

// Value in range 0-100 that dictates the encoding quality for jpeg
// format.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<int> kEncodingQualityJpeg;

// Value in range 0-100 that dictates the encoding quality for webp
// format.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<int> kEncodingQualityWebp;

// The URL to send pings for Lens Standalone.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<std::string> kLensPingURL;

// Enables sequential processing of the Lens Ping before performing the
// Lens Standalone search. If false, the ping will occur in parallel to
// the search.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kPingLensSequentially;

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLatencyLogging();

// Returns whether the image search side panel is supported for third party
// default search engines
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableImageSearchUnifiedSidePanelFor3PDse();

// Returns the max pixel width/height for the image to be sent to Lens via
// region search. The images are sent at 1x as PNGs.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxPixelsForRegionSearch();

// Returns the max area for the image to be sent to Lens via region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxAreaForRegionSearch();

// Returns the max pixel width/height for the image to be sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxPixelsForImageSearch();

// The URL for the Lens home page.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetHomepageURLForLens();

// Returns whether to apply fix for HTML redirects.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensHtmlRedirectFix();

// Returns whether to remove the Lens side panel loading state in the
// OnDocumentOnLoadCompletedInPrimaryMainFrame web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame();

// Returns whether to remove the Lens side panel loading state in the
// DOMContentLoaded web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDomContentLoaded();

// Returns whether to remove the Lens side panel loading state in the
// DidFinishNavigation web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDidFinishNavigation();

// Returns whether to remove the Lens side panel loading state in the
// NavigationEntryCommitted web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnNavigationEntryCommitted();

// Returns whether to remove the Lens side panel loading state in the
// DidFinishLoad web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDidFinishLoad();

// Returns whether to remove the Lens side panel loading state in the
// PrimaryPageChanged web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnPrimaryPageChanged();

// Returns whether Lens fullscreen search is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensFullscreenSearchEnabled();

// Returns whether the Lens side panel is enabled for image search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelEnabled();

// Returns whether the Lens side panel is enabled for region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelEnabledForRegionSearch();

// Returns whether the Search Image button in the Chrome Screenshot Sharing
// feature is enabled
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensInScreenshotSharingEnabled();

// Returns whether we should use a WebUI static page for region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensRegionSearchStaticPageEnabled();

// Returns whether to use WebP encoding for image search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsWebpForImageSearchEnabled();

// Get the encoding quality for jpeg search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetEncodingQualityJpeg();

// Get the encoding quality for webp search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetEncodingQualityWebp();

// Returns whether to use WebP encoding for region search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsWebpForRegionSearchEnabled();

// Returns whether to use JPEG encoding for region search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsJpegForRegionSearchEnabled();

// Returns whether to enable the context menu in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableContextMenuInLensSidePanel();

// Returns whether to ping Lens before sending a Standalone request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensPing();

// The URL for the Lens ping.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensPingURL();

// Returns whether or not the Lens ping should be done sequentially.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensPingIsSequential();

// Returns whether to issue Lens preconnect requests when the
// context menu item is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldIssuePreconnectForLens();

// Returns the preconnect url to use for when the context menu item
// is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetPreconnectKeyForLens();

// Returns whether to start a Spare Render process when the context menu item
// is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldIssueProcessPrewarmingForLens();
}  // namespace features
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
