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

#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "cobalt/android/cobalt_process_state_summary_jni.h"

namespace cobalt {

void CobaltProcessStateSummaryManager::SyncToSystemServer(
    const std::vector<uint8_t>& payload) {
  CobaltProcessStateSummarySyncToSystemServer(payload);
}

std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::ReadPriorSessionSnapshot() {
  std::optional<std::vector<uint8_t>> buffer =
      CobaltProcessStateSummaryGetPriorSessionSummary();
  if (!buffer) {
    return std::nullopt;
  }
  return DeserializeSnapshot(*buffer);
}

int CobaltProcessStateSummaryManager::RecordLatestExitReasonToUma(
    const std::string& uma_name) {
  int exit_reason =
      CobaltProcessStateSummaryRecordLatestExitReasonToUma(uma_name);
  if (exit_reason >= 0 && !uma_name.empty()) {
    int sample =
        std::min(exit_reason, static_cast<int>(AndroidExitReason::kMaxValue));
    base::UmaHistogramExactLinear(
        uma_name, sample, static_cast<int>(AndroidExitReason::kMaxValue) + 1);
  }
  return exit_reason;
}

std::optional<ProcessStateSnapshot> CobaltProcessStateSummaryManager::
    RecordLatestExitReasonAndGetPriorSessionSnapshot(
        const std::string& uma_name,
        int* out_exit_reason) {
  std::optional<std::vector<uint8_t>> buffer =
      CobaltProcessStateSummaryRecordLatestExitReasonAndGetSummary(
          uma_name, out_exit_reason);
  if (out_exit_reason && *out_exit_reason >= 0 && !uma_name.empty()) {
    int sample = std::min(*out_exit_reason,
                          static_cast<int>(AndroidExitReason::kMaxValue));
    base::UmaHistogramExactLinear(
        uma_name, sample, static_cast<int>(AndroidExitReason::kMaxValue) + 1);
  }
  if (!buffer) {
    return std::nullopt;
  }
  return DeserializeSnapshot(*buffer);
}

}  // namespace cobalt
