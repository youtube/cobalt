// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_utils.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

bool IsPrefetchActivationBeaconEnabled(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  auto override_state = base::FeatureList::GetStateIfOverridden(
      features::kPrefetchActivationBeacon);
  if (override_state.has_value() && !override_state.value()) {
    // Explicitly disabled by kill switch.
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kPrefetchActivationBeacon)) {
    // Enabled by Finch or default.
    return true;
  }
  // Otherwise, check if origin trial is enabled via response headers.
  return response_headers &&
         blink::TrialTokenValidator().RequestEnablesFeature(
             request_url, response_headers,
             "PrefetchAndPrerenderActivationBeacon", base::Time::Now());
}

bool IsPrerenderActivationBeaconEnabled(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  auto override_state = base::FeatureList::GetStateIfOverridden(
      features::kPrerenderActivationBeacon);
  if (override_state.has_value() && !override_state.value()) {
    // Explicitly disabled by kill switch.
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kPrerenderActivationBeacon)) {
    // Enabled by Finch or default.
    return true;
  }
  // Otherwise, check if origin trial is enabled via response headers.
  return response_headers &&
         blink::TrialTokenValidator().RequestEnablesFeature(
             request_url, response_headers,
             "PrefetchAndPrerenderActivationBeacon", base::Time::Now());
}

}  // namespace content
