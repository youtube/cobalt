// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_METRICS_QUICK_INSERT_FEATURE_USAGE_METRICS_H_
#define ASH_QUICK_INSERT_METRICS_QUICK_INSERT_FEATURE_USAGE_METRICS_H_

#include "ash/ash_export.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {

// Tracks Quick Insert feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class ASH_EXPORT QuickInsertFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  QuickInsertFeatureUsageMetrics();
  ~QuickInsertFeatureUsageMetrics() override;

  // Starts recording the usage time of the feature.
  void StartUsage();

  // Stops recording the usage time of the feature.
  void StopUsage();

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  std::optional<bool> IsAccessible() const override;
  bool IsEnabled() const override;

 private:
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_METRICS_QUICK_INSERT_FEATURE_USAGE_METRICS_H_
