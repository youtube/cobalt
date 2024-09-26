// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_

#include "base/feature_list.h"
#include "base/time/time.h"

BASE_DECLARE_FEATURE(kSearchPrefetchServicePrefetching);

BASE_DECLARE_FEATURE(kSearchPrefetchBlockBeforeHeaders);

BASE_DECLARE_FEATURE(kSearchPrefetchSkipsCancel);

BASE_DECLARE_FEATURE(kSearchPrefetchOnlyAllowDefaultMatchPreloading);

// Whether matching prefetches can block navigation until they are determined to
// be serve-able or not based on headers.
bool SearchPrefetchBlockBeforeHeadersIsEnabled();

// Whether the search prefetch service actually initiates prefetches.
bool SearchPrefetchServicePrefetchingIsEnabled();

// The amount of time a response is considered valid after the prefetch starts.
base::TimeDelta SearchPrefetchCachingLimit();

// The number of prefetches that can be initiated (but not served) within a time
// period as long as |SearchPrefetchCachingLimit()|
size_t SearchPrefetchMaxAttemptsPerCachingDuration();

// The amount of time that a network error will cause the search prefetch
// service to stop prefetching responses.
base::TimeDelta SearchPrefetchErrorBackoffDuration();

// The max number of stored cached prefetch responses. This is stored as a list
// of navigation URLs to prefetch URLs.
size_t SearchPrefetchMaxCacheEntries();

// The amount of time that needs to have elapsed before we consider a prefetch
// eligible to be served.
base::TimeDelta SearchPrefetchBlockHeadStart();

BASE_DECLARE_FEATURE(kSearchNavigationPrefetch);

// An experimental feature to measure if starting search prefetches during
// navigation events provides benefit over the typical navigation flow.
bool IsSearchNavigationPrefetchEnabled();

// An experimental feature that skips the cancellation logic in search prefetch
// service.
bool SearchPrefetchSkipsCancel();

// A flavor of navigation prefetch that triggers when the user changes the
// selected index in omnibox to a search suggestion via arrow buttons.
bool IsUpOrDownArrowPrefetchEnabled();

// A flavor of navigation prefetch that triggers when the user pushes the mouse
// down on a Search suggestion.
bool IsSearchMouseDownPrefetchEnabled();

// Allows the top selection to be prefetched by navigation prefetch strategies.
bool AllowTopNavigationPrefetch();

// Allows search history suggestions to be prefetched by navigation prefetch
// strategies.
bool PrefetchSearchHistorySuggestions();

// Whether Omnibox prefetch and prerender should be restricted to the suggestion
// being the default match.
bool OnlyAllowDefaultMatchPreloading();

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
