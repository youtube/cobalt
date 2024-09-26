// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// See https://wicg.github.io/attribution-reporting-api/#vendor-specific-values
// for details.
struct CONTENT_EXPORT AttributionConfig {
  // Controls rate limits for the API.
  struct CONTENT_EXPORT RateLimitConfig {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls the rate-limiting time window for attribution.
    base::TimeDelta time_window = base::Days(30);

    // Maximum number of distinct reporting origins that can register sources
    // for a given <source site, destination site> in `time_window`.
    int64_t max_source_registration_reporting_origins = 100;

    // Maximum number of distinct reporting origins that can create attributions
    // for a given <source site, destination site> in `time_window`.
    int64_t max_attribution_reporting_origins = 10;

    // Maximum number of attributions for a given <source site, destination
    // site, reporting origin> in `time_window`.
    int64_t max_attributions = 100;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  struct EventLevelLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls the valid range of trigger data.
    uint64_t navigation_source_trigger_data_cardinality = 8;
    uint64_t event_source_trigger_data_cardinality = 2;

    // Controls randomized response rates for the API: when a source is
    // registered, this parameter is used to determine the probability that any
    // subsequent attributions for the source are handled truthfully, or whether
    // the source is immediately attributed with zero or more fake reports and
    // real attributions are dropped. Must be non-negative and non-NaN, but may
    // be infinite.
    double randomized_response_epsilon = 14;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Controls how many times a single source can create an event-level report.
    int max_attributions_per_navigation_source = 3;
    int max_attributions_per_event_source = 1;

    // Default constants for report window deadlines.
    static constexpr base::TimeDelta kDefaultFirstReportWindowDeadline =
        base::Days(2);
    static constexpr base::TimeDelta kDefaultSecondReportWindowDeadline =
        base::Days(7);

    // Controls the report window deadlines for scheduling report times.
    base::TimeDelta first_report_window_deadline =
        kDefaultFirstReportWindowDeadline;
    base::TimeDelta second_report_window_deadline =
        kDefaultSecondReportWindowDeadline;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  struct AggregateLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Controls the maximum sum of the contributions (values) across all buckets
    // per source.
    // When updating the value, the corresponding BUDGET_PER_SOURCE value in
    // //content/browser/resources/attribution_reporting/attribution_internals.ts
    // should also be updated.
    int64_t aggregatable_budget_per_source = 65536;

    // Default constants for the report delivery time to be used when declaring
    // field trial params.
    static constexpr base::TimeDelta kDefaultMinDelay = base::Minutes(10);
    static constexpr base::TimeDelta kDefaultDelaySpan = base::Minutes(50);

    // Controls the report delivery time.
    base::TimeDelta min_delay = kDefaultMinDelay;
    base::TimeDelta delay_span = kDefaultDelaySpan;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  // Returns true if this config is valid.
  [[nodiscard]] bool Validate() const;

  // Controls how many sources can be in the storage per source origin.
  int max_sources_per_origin = 1024;

  // Controls the maximum number of distinct attribution destinations that can
  // be in storage at any time for sources with the same <source site, reporting
  // origin>.
  int max_destinations_per_source_site_reporting_origin = 100;

  RateLimitConfig rate_limit;
  EventLevelLimit event_level_limit;
  AggregateLimit aggregate_limit;

  // When adding new members, the corresponding `Validate()` definition and
  // `operator==()` definition in `attribution_interop_parser_unittest.cc`
  // should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
