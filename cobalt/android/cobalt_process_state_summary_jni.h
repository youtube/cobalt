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

#ifndef COBALT_ANDROID_COBALT_PROCESS_STATE_SUMMARY_JNI_H_
#define COBALT_ANDROID_COBALT_PROCESS_STATE_SUMMARY_JNI_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"

namespace cobalt {
namespace android {

// Sets the Android OS process state summary via ActivityManager API.
void SetProcessStateSummary(base::span<const uint8_t> summary_bytes);

// Retrieves the process state summary recorded for the latest prior session.
std::optional<std::vector<uint8_t>> GetPriorSessionProcessStateSummary();

// Consolidates querying the latest exit reason and retrieving the prior
// session process state summary in a single Binder transaction.
std::optional<std::vector<uint8_t>> RecordLatestExitReasonAndGetSummary(
    const std::string& uma_name,
    int* out_exit_reason);

}  // namespace android
}  // namespace cobalt

#endif  // COBALT_ANDROID_COBALT_PROCESS_STATE_SUMMARY_JNI_H_
