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
#include "base/numerics/safe_conversions.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"

namespace cobalt {

CobaltCpuMetricsEmitter::CobaltCpuMetricsEmitter()
    : process_metrics_(base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle())) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CobaltCpuMetricsEmitter::~CobaltCpuMetricsEmitter() = default;

void CobaltCpuMetricsEmitter::FetchAndEmitCpuMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Total CPU utilization in percentage of all cores in between every call.
  // This will be zero on first call or when GetPlatformIndependentCPUUsage returns on error.
  const double cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage().value_or(0.0);
  // Divide by the number of processors to log average per core CPU usage.
  base::UmaHistogramPercentage("CPU.Total.UsageInPercentage",
                               base::ClampRound<int>(cpu_usage / base::SysInfo::NumberOfProcessors()));
}

}  // namespace cobalt
