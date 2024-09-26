// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_config.h"

#include <cmath>

#include "base/time/time.h"

namespace content {

bool AttributionConfig::Validate() const {
  if (max_sources_per_origin <= 0) {
    return false;
  }

  if (max_destinations_per_source_site_reporting_origin <= 0) {
    return false;
  }

  if (!rate_limit.Validate()) {
    return false;
  }

  if (!event_level_limit.Validate()) {
    return false;
  }

  if (!aggregate_limit.Validate()) {
    return false;
  }

  return true;
}

bool AttributionConfig::RateLimitConfig::Validate() const {
  if (time_window <= base::TimeDelta()) {
    return false;
  }

  if (max_source_registration_reporting_origins <= 0) {
    return false;
  }

  if (max_attribution_reporting_origins <= 0) {
    return false;
  }

  if (max_attributions <= 0) {
    return false;
  }

  return true;
}

bool AttributionConfig::EventLevelLimit::Validate() const {
  if (navigation_source_trigger_data_cardinality == 0u) {
    return false;
  }

  if (event_source_trigger_data_cardinality == 0u) {
    return false;
  }

  if (max_reports_per_destination <= 0) {
    return false;
  }

  if (max_attributions_per_navigation_source <= 0) {
    return false;
  }

  if (max_attributions_per_event_source <= 0) {
    return false;
  }

  if (randomized_response_epsilon < 0 ||
      std::isnan(randomized_response_epsilon)) {
    return false;
  }

  if (first_report_window_deadline < base::TimeDelta() ||
      second_report_window_deadline <= first_report_window_deadline) {
    return false;
  }

  return true;
}

bool AttributionConfig::AggregateLimit::Validate() const {
  if (max_reports_per_destination <= 0) {
    return false;
  }

  if (aggregatable_budget_per_source <= 0) {
    return false;
  }

  if (min_delay < base::TimeDelta()) {
    return false;
  }

  if (delay_span < base::TimeDelta()) {
    return false;
  }

  return true;
}

}  // namespace content
