// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom.h"

namespace attribution_reporting {

constexpr size_t kMaxBytesPerFilterString = 25;
constexpr size_t kMaxValuesPerFilter = 50;
constexpr size_t kMaxFiltersPerSource = 50;

constexpr size_t kMaxDestinations = 3;

constexpr size_t kMaxEventLevelReportWindows = 5;

constexpr size_t kMaxBytesPerAggregationKeyId = 25;
constexpr size_t kMaxAggregationKeysPerSource = 20;

constexpr int kMaxAggregatableValue = 65536;

constexpr int kMaxSettableEventLevelAttributions = 20;

constexpr base::TimeDelta kMinSourceExpiry = base::Days(1);
constexpr base::TimeDelta kMaxSourceExpiry = base::Days(30);

static_assert(kMinSourceExpiry < kMaxSourceExpiry);

constexpr base::TimeDelta kMinReportWindow = base::Hours(1);

static_assert(kMinReportWindow <= kMinSourceExpiry);

constexpr uint32_t DefaultTriggerDataCardinality(
    mojom::SourceType source_type) {
  switch (source_type) {
    case mojom::SourceType::kNavigation:
      return 8;
    case mojom::SourceType::kEvent:
      return 2;
  }
}

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
