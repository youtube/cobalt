// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_DISPLAY_SIZE_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_DISPLAY_SIZE_UTILS_H_

#include <string>
#include <tuple>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"

namespace blink {

BLINK_COMMON_EXPORT std::string ConvertAdSizeUnitToString(
    const blink::AdSize::LengthUnit& unit);

// Helper function that converts a size string into its corresponding value and
// units. Accepts measurements in pixels (px) and screen widths (sw).
// Examples of allowed inputs: "200px" "200 px" "50sw" "25         sw"
BLINK_COMMON_EXPORT std::tuple<double, blink::AdSize::LengthUnit>
ParseAdSizeString(const base::StringPiece input);

BLINK_COMMON_EXPORT bool IsValidAdSize(const blink::AdSize& size);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INTEREST_GROUP_AD_DISPLAY_SIZE_UTILS_H_
