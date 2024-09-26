// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

#import <stdint.h>

#import "components/prefs/pref_service.h"
#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/application_context/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const bool* g_metrics_consent_for_testing = nullptr;

}  // namespace

// static
void IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
    const bool* value) {
  DCHECK_NE(g_metrics_consent_for_testing == nullptr, value == nullptr)
      << "Unpaired set/reset";

  g_metrics_consent_for_testing = value;
}

// static
bool IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() {
  if (g_metrics_consent_for_testing)
    return *g_metrics_consent_for_testing;

  return IsMetricsReportingEnabled(GetApplicationContext()->GetLocalState());
}

// static
bool IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name,
    variations::SyntheticTrialAnnotationMode annotation_mode) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      GetApplicationContext()->GetMetricsService(), trial_name, group_name,
      annotation_mode);
}
