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

namespace cobalt {

void CobaltProcessStateSummaryManager::UpdateMemoryFootprint(
    uint32_t current_rss_kb,
    uint32_t current_pmf_kb,
    uint32_t current_v8_code_kb) {}

void CobaltProcessStateSummaryManager::UpdateTrimMemoryLevel(uint8_t level) {}

void CobaltProcessStateSummaryManager::SetFlags(uint8_t flags) {}

void CobaltProcessStateSummaryManager::SetStartupMilestone(uint8_t milestone) {}

void CobaltProcessStateSummaryManager::SetStartupGuardArmed(bool is_armed) {}

void CobaltProcessStateSummaryManager::SetStartupGuardTriggeredKill() {}

std::optional<std::vector<uint8_t>>
CobaltProcessStateSummaryManager::PreparePayloadLocked() {
  return std::nullopt;
}

void CobaltProcessStateSummaryManager::SyncToSystemServer(
    const std::vector<uint8_t>& payload) {}

std::optional<ProcessStateSnapshot>
CobaltProcessStateSummaryManager::ReadPriorSessionSnapshot() {
  return std::nullopt;
}

int CobaltProcessStateSummaryManager::RecordLatestExitReasonToUma(
    const std::string& uma_name) {
  return -1;
}

std::optional<ProcessStateSnapshot> CobaltProcessStateSummaryManager::
    RecordLatestExitReasonAndGetPriorSessionSnapshot(
        const std::string& uma_name,
        int* out_exit_reason) {
  if (out_exit_reason) {
    *out_exit_reason = -1;
  }
  return std::nullopt;
}

}  // namespace cobalt
