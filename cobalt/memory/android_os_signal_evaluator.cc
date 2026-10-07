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

#include "cobalt/memory/android_os_signal_evaluator.h"

#include <utility>

#include "base/android/memory_pressure_listener_android.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"

namespace cobalt {
namespace memory {

namespace {
AndroidOsSignalEvaluator* g_instance = nullptr;
}  // namespace

AndroidOsSignalEvaluator::AndroidOsSignalEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter)
    : ::memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)) {
  DCHECK(!g_instance);
  g_instance = this;

  base::android::MemoryPressureListenerAndroid::SetMemoryPressureForwarderCallback(
      base::BindRepeating(&AndroidOsSignalEvaluator::OnMemoryPressure,
                          base::Unretained(this)));
  LOG(INFO) << "[CobaltMemoryPressure] AndroidOsSignalEvaluator initialized "
               "and registered JNI forwarder";
}

AndroidOsSignalEvaluator::~AndroidOsSignalEvaluator() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  base::android::MemoryPressureListenerAndroid::SetMemoryPressureForwarderCallback(
      base::NullCallback());
  LOG(INFO) << "[CobaltMemoryPressure] AndroidOsSignalEvaluator destroyed";
}

// static
AndroidOsSignalEvaluator* AndroidOsSignalEvaluator::GetInstance() {
  return g_instance;
}

void AndroidOsSignalEvaluator::OnMemoryPressure(MemoryPressureLevel level) {
  SetCurrentVote(level);
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    SendCurrentVote(/*notify=*/false);
    return;
  }

  LOG(INFO) << "[CobaltMemoryPressure] AndroidOsSignalEvaluator casting vote="
            << level << " (notify=true)";
  SendCurrentVote(/*notify=*/true);

  // Android OS trim signals are one-shot events with no recovery callback.
  // Reset this voter back to NONE without notifying after dispatching so it
  // does not leave a sticky vote in MemoryPressureVoteAggregator.
  SetCurrentVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  SendCurrentVote(/*notify=*/false);
}

}  // namespace memory
}  // namespace cobalt
