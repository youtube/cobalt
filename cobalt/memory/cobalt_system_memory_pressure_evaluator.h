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

#ifndef COBALT_MEMORY_COBALT_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
#define COBALT_MEMORY_COBALT_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/memory_pressure/memory_pressure_voter.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace cobalt {
namespace memory {

namespace switches {
inline constexpr char kProcessMemoryBudgetMB[] = "process-memory-budget-mb";
}  // namespace switches

// Dual-heuristic memory pressure evaluator for Cobalt on Linux, RDK, and
// Starboard platforms.
//
// Primary Heuristic (Process-Budget):
//   Evaluates Cobalt's own private anonymous footprint against a target
//   process memory budget. Casts MODERATE at 85% of budget and CRITICAL at 95%.
//
// Secondary Heuristic (System-Available):
//   Evaluates global OS available memory via /proc/meminfo. Casts MODERATE when
//   available memory drops below 30% and CRITICAL when below 15%.
//
// The effective vote is std::max(process_level, system_level).
class CobaltSystemMemoryPressureEvaluator
    : public ::memory_pressure::SystemMemoryPressureEvaluator {
 public:
  static constexpr float kDefaultModerateMemoryFraction = 0.30f;
  static constexpr float kDefaultCriticalMemoryFraction = 0.15f;
  static constexpr float kDefaultModerateBudgetRatio = 0.85f;
  static constexpr float kDefaultCriticalBudgetRatio = 0.95f;
  static constexpr int kDefaultBudgetMbLowEnd = 180;
  static constexpr int kDefaultBudgetMbStandard = 200;
  static constexpr base::TimeDelta kDefaultPollInterval = base::Seconds(5);
  static constexpr base::TimeDelta kDefaultCooldown = base::Seconds(15);

  using ProcessMemoryInfoGetter = base::RepeatingCallback<
      base::expected<base::ProcessMemoryInfo, base::ProcessUsageError>()>;
  using SystemMemoryInfoGetter =
      base::RepeatingCallback<bool(base::SystemMemoryInfoKB*)>;
  using MediaAllowanceGetter = base::RepeatingCallback<uint64_t()>;

  explicit CobaltSystemMemoryPressureEvaluator(
      std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
      MediaAllowanceGetter media_allowance_getter = {});

  // Constructor for testing with mock getters, media allowance, and custom
  // thresholds.
  CobaltSystemMemoryPressureEvaluator(
      std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
      ProcessMemoryInfoGetter process_memory_info_getter,
      SystemMemoryInfoGetter system_memory_info_getter,
      MediaAllowanceGetter media_allowance_getter,
      uint64_t process_memory_budget_bytes,
      float moderate_budget_ratio,
      float critical_budget_ratio,
      float moderate_system_fraction,
      float critical_system_fraction,
      base::TimeDelta poll_interval,
      base::TimeDelta cooldown);

  CobaltSystemMemoryPressureEvaluator(
      std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
      ProcessMemoryInfoGetter process_memory_info_getter,
      SystemMemoryInfoGetter system_memory_info_getter,
      uint64_t process_memory_budget_bytes,
      float moderate_budget_ratio,
      float critical_budget_ratio,
      float moderate_system_fraction,
      float critical_system_fraction,
      base::TimeDelta poll_interval,
      base::TimeDelta cooldown);

  CobaltSystemMemoryPressureEvaluator(
      const CobaltSystemMemoryPressureEvaluator&) = delete;
  CobaltSystemMemoryPressureEvaluator& operator=(
      const CobaltSystemMemoryPressureEvaluator&) = delete;

  ~CobaltSystemMemoryPressureEvaluator() override;

  // Evaluates memory pressure immediately and updates votes.
  void CheckMemoryPressure();

  // Stops periodic polling.
  void Stop();

  base::MemoryPressureListener::MemoryPressureLevel
  CalculateCurrentPressureLevel();

  base::MemoryPressureListener::MemoryPressureLevel
  CalculateProcessPressureLevel();

  base::MemoryPressureListener::MemoryPressureLevel
  CalculateSystemPressureLevel();

  uint64_t process_memory_budget_bytes() const {
    return process_memory_budget_bytes_;
  }

 private:
  void Start();
  void UpdateMemoryPressureLevel(
      base::MemoryPressureListener::MemoryPressureLevel new_level);

  std::unique_ptr<base::ProcessMetrics> process_metrics_;
  ProcessMemoryInfoGetter process_memory_info_getter_;
  SystemMemoryInfoGetter system_memory_info_getter_;
  MediaAllowanceGetter media_allowance_getter_;
  const uint64_t process_memory_budget_bytes_;
  const float moderate_budget_ratio_;
  const float critical_budget_ratio_;
  const float moderate_system_fraction_;
  const float critical_system_fraction_;
  const base::TimeDelta poll_interval_;
  const base::TimeDelta cooldown_;

  int repeat_count_ = 0;
  base::RepeatingTimer timer_;
  base::WeakPtrFactory<CobaltSystemMemoryPressureEvaluator> weak_ptr_factory_{
      this};
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_COBALT_SYSTEM_MEMORY_PRESSURE_EVALUATOR_H_
