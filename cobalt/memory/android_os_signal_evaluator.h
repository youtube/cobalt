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

#ifndef COBALT_MEMORY_ANDROID_OS_SIGNAL_EVALUATOR_H_
#define COBALT_MEMORY_ANDROID_OS_SIGNAL_EVALUATOR_H_

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace cobalt {
namespace memory {

// Evaluator that ingests Android OS lifecycle memory signals (from JNI
// onMemoryPressure) and translates them into Chromium MemoryPressureLevel votes
// for the MultiSourceMemoryPressureMonitor.
class AndroidOsSignalEvaluator
    : public ::memory_pressure::SystemMemoryPressureEvaluator {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  explicit AndroidOsSignalEvaluator(
      std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter);
  ~AndroidOsSignalEvaluator() override;

  AndroidOsSignalEvaluator(const AndroidOsSignalEvaluator&) = delete;
  AndroidOsSignalEvaluator& operator=(const AndroidOsSignalEvaluator&) = delete;

  // Singleton instance accessor for JNI forwarding.
  static AndroidOsSignalEvaluator* GetInstance();

  // Called when Android sends a memory pressure notification over JNI.
  void OnMemoryPressure(MemoryPressureLevel level);
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_ANDROID_OS_SIGNAL_EVALUATOR_H_
