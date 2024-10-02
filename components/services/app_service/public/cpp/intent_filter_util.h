// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_

// Utility functions for creating an App Service intent filter.

#include <string>

#include "components/services/app_service/public/cpp/intent_filter.h"
#include "url/gurl.h"

namespace apps_util {

// Create intent filter for URL scope, with prefix matching only for the path.
// e.g. filter created for https://www.google.com/ will match any URL that
// started with https://www.google.com/*.

// TODO(crbug.com/1092784): Update/add all related unit tests to test with
// action view.
apps::IntentFilterPtr MakeIntentFilterForUrlScope(const GURL& url);

// Check if the two intent filters have overlap. i.e. they can handle same
// intent with same match level.
bool FiltersHaveOverlap(const apps::IntentFilterPtr& filter1,
                        const apps::IntentFilterPtr& filter2);

// Upgrade the filter to contain action view.
void UpgradeFilter(apps::IntentFilterPtr& filter);

// Checks if the `intent_filter` is a supported link for `app_id`, i.e. it has
// the "view" action, a http or https scheme, and at least one host and pattern.
bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::IntentFilterPtr& intent_filter);

// If |intent_filter| matches a URL by prefix, return the length of the longest
// match. For example, if the filter matches "https://example.org/app/*", the
// longest match for the URL "https://example.org/app/foo/bar" is the length of
// "https://example.org/app/" (24). If |intent_filter| does not match |url|, or
// if the filter does not match a prefix (e.g. glob), 0 is returned.
size_t IntentFilterUrlMatchLength(const apps::IntentFilterPtr& intent_filter,
                                  const GURL& url);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
