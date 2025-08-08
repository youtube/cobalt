// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_

#include "base/feature_list.h"
#include "components/commerce/core/commerce_types.h"

class GURL;

namespace commerce {
// Returns whether the `url` contains the discount utm tags.
bool UrlContainsDiscountUtmTag(const GURL& url);

// Gets test data for the parcel tracking APIs if the |kParcelTrackingTestData|
// flag is enabled.
ParcelTrackingStatus GetParcelTrackingStatusTestData();

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
