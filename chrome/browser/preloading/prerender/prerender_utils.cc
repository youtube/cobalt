// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_utils.h"

#include "chrome/browser/browser_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"

namespace prerender_utils {

BASE_FEATURE(kHidePrefetchParameter,
             "HidePrefetchParameter",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If you add a new type of prerender trigger, please refer to the internal
// document go/update-prerender-new-trigger-metrics to make sure that metrics
// include the newly added trigger type.
// LINT.IfChange
const char kDefaultSearchEngineMetricSuffix[] = "DefaultSearchEngine";
const char kDirectUrlInputMetricSuffix[] = "DirectURLInput";
const char kBookmarkBarMetricSuffix[] = "BookmarkBar";
const char kNewTabPageMetricSuffix[] = "NewTabPage";
const char kLinkPreviewMetricsSuffix[] = "LinkPreview";
// LINT.ThenChange()

bool IsDirectUrlInputPrerenderEnabled() {
  return base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2);
}

bool IsSearchSuggestionPrerenderEnabled() {
  return base::FeatureList::IsEnabled(
      features::kSupportSearchSuggestionForPrerender2);
}

bool ShouldUpdateCacheEntryManually() {
  return base::FeatureList::IsEnabled(kHidePrefetchParameter);
}

bool SearchPreloadShareableCacheIsEnabled() {
  switch (features::kSearchPreloadShareableCacheTypeParam.Get()) {
    case features::SearchPreloadShareableCacheType::kEnabled:
      return true;
    case features::SearchPreloadShareableCacheType::kDisabled:
      return false;
  }
}

}  // namespace prerender_utils
