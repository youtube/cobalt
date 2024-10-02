// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/discover_feed_experiments_tracker.h"

#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedExperimentsTracker::DiscoverFeedExperimentsTracker(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  RegisterExperiments(feed::prefs::GetExperiments(*pref_service_));
}

DiscoverFeedExperimentsTracker::~DiscoverFeedExperimentsTracker() {}

void DiscoverFeedExperimentsTracker::UpdateExperiments(
    const feed::Experiments& experiments) {
  RegisterExperiments(experiments);
  feed::prefs::SetExperiments(experiments, *pref_service_);
}

void DiscoverFeedExperimentsTracker::RegisterExperiments(
    const feed::Experiments& experiments) {
  // Note that this does not affect the contents of the X-Client-Data
  // by design. We do not provide the variations IDs from the backend
  // and do not attach them to the X-Client-Data header.
  for (const auto& exp : experiments) {
    for (const auto& group_name : exp.second) {
      IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(exp.first,
                                                                   group_name);
    }
  }
}
