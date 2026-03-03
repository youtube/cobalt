// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_cpu_metrics_emitter.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"

namespace cobalt {

CobaltCpuMetricsEmitter::CobaltCpuMetricsEmitter()
    : process_metrics_(base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CobaltCpuMetricsEmitter::~CobaltCpuMetricsEmitter() = default;

void CobaltCpuMetricsEmitter::FetchAndEmitCpuMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static base::TimeDelta last_cumulative_cpu_time_ = base::TimeDelta(); 
  base::TimeDelta cumulative_cpu_time = process_metrics_->GetCumulativeCPUUsage();

  if (!last_cumulative_cpu_time_.is_zero()) {
    // use cpu usage delta?
    base::TimeDelta total_delta = cumulative_cpu_time - last_cumulative_cpu_time_;
    base::UmaHistogramLongTimes("CPU.Total.Usage", total_delta);
  }
  last_cumulative_cpu_time_ = cumulative_cpu_time;
}

}  // namespace cobalt
