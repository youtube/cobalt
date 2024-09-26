// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_posix.h"

#include "base/notreached.h"

namespace performance_monitor {

MetricEvaluatorsHelperPosix::MetricEvaluatorsHelperPosix() = default;
MetricEvaluatorsHelperPosix::~MetricEvaluatorsHelperPosix() = default;

absl::optional<int> MetricEvaluatorsHelperPosix::GetFreePhysicalMemoryMb() {
  NOTREACHED();
  return absl::nullopt;
}

}  // namespace performance_monitor
