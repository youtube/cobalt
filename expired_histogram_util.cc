// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histogram_util.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/statistics_recorder.h"
#include "components/metrics/expired_histograms_checker.h"

namespace metrics {
namespace {

const base::Feature kExpiredHistogramLogicFeature{
    "ExpiredHistogramLogic", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kWhitelistParam{
    &kExpiredHistogramLogicFeature, "whitelist", ""};

}  // namespace

void EnableExpiryChecker(const uint64_t* expired_histograms_hashes,
                         size_t num_expired_histograms) {
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(kExpiredHistogramLogicFeature)) {
    base::StatisticsRecorder::SetRecordChecker(
        std::make_unique<ExpiredHistogramsChecker>(expired_histograms_hashes,
                                                   num_expired_histograms,
                                                   kWhitelistParam.Get()));
  }
}

}  // namespace metrics