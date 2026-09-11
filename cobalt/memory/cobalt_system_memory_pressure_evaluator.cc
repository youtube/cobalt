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

#include "cobalt/memory/cobalt_system_memory_pressure_evaluator.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace cobalt {
namespace memory {

namespace {

constexpr char kSwitchModerateFraction[] =
    "cobalt-memory-pressure-moderate-fraction";
constexpr char kSwitchCriticalFraction[] =
    "cobalt-memory-pressure-critical-fraction";
constexpr char kSwitchModerateBudgetRatio[] =
    "cobalt-memory-pressure-moderate-budget-ratio";
constexpr char kSwitchCriticalBudgetRatio[] =
    "cobalt-memory-pressure-critical-budget-ratio";
constexpr char kSwitchPollIntervalMs[] =
    "cobalt-memory-pressure-poll-interval-ms";
constexpr char kSwitchCooldownMs[] = "cobalt-memory-pressure-cooldown-ms";

float GetSwitchValueFloat(const char* switch_name, float default_value) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name)) {
    double value;
    if (base::StringToDouble(command_line->GetSwitchValueASCII(switch_name),
                             &value) &&
        value >= 0.0 && value <= 1.0) {
      return static_cast<float>(value);
    }
  }
  return default_value;
}

base::TimeDelta GetSwitchValueTimeDeltaMs(const char* switch_name,
                                          base::TimeDelta default_value) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name)) {
    int value;
    if (base::StringToInt(command_line->GetSwitchValueASCII(switch_name),
                          &value) &&
        value > 0) {
      return base::Milliseconds(value);
    }
  }
  return default_value;
}

uint64_t ResolveProcessMemoryBudget(
    const CobaltSystemMemoryPressureEvaluator::SystemMemoryInfoGetter&
        sys_info_getter) {
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd && cmd->HasSwitch(switches::kProcessMemoryBudgetMB)) {
    int budget_mb = 0;
    if (base::StringToInt(
            cmd->GetSwitchValueASCII(switches::kProcessMemoryBudgetMB),
            &budget_mb) &&
        budget_mb > 0) {
      return static_cast<uint64_t>(budget_mb) * 1024 * 1024;
    }
  }

  base::SystemMemoryInfoKB sys_info;
  if (sys_info_getter && sys_info_getter.Run(&sys_info) && sys_info.total > 0) {
    if (sys_info.total <= 1024 * 1024) {
      return static_cast<uint64_t>(
                 CobaltSystemMemoryPressureEvaluator::kDefaultBudgetMbLowEnd) *
             1024 * 1024;
    }
  }
  return static_cast<uint64_t>(
             CobaltSystemMemoryPressureEvaluator::kDefaultBudgetMbStandard) *
         1024 * 1024;
}

}  // namespace

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    MediaAllowanceGetter media_allowance_getter)
    : CobaltSystemMemoryPressureEvaluator(
          std::move(voter),
          /*process_memory_info_getter=*/{},
          base::BindRepeating(&base::GetSystemMemoryInfo),
          std::move(media_allowance_getter),
          /*process_memory_budget_bytes=*/0,
          GetSwitchValueFloat(kSwitchModerateBudgetRatio,
                              kDefaultModerateBudgetRatio),
          GetSwitchValueFloat(kSwitchCriticalBudgetRatio,
                              kDefaultCriticalBudgetRatio),
          GetSwitchValueFloat(kSwitchModerateFraction,
                              kDefaultModerateMemoryFraction),
          GetSwitchValueFloat(kSwitchCriticalFraction,
                              kDefaultCriticalMemoryFraction),
          GetSwitchValueTimeDeltaMs(kSwitchPollIntervalMs,
                                    kDefaultPollInterval),
          GetSwitchValueTimeDeltaMs(kSwitchCooldownMs, kDefaultCooldown)) {}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    ProcessMemoryInfoGetter process_memory_info_getter,
    SystemMemoryInfoGetter system_memory_info_getter,
    uint64_t process_memory_budget_bytes,
    float moderate_budget_ratio,
    float critical_budget_ratio,
    float moderate_system_fraction,
    float critical_system_fraction,
    base::TimeDelta poll_interval,
    base::TimeDelta cooldown)
    : CobaltSystemMemoryPressureEvaluator(std::move(voter),
                                          std::move(process_memory_info_getter),
                                          std::move(system_memory_info_getter),
                                          /*media_allowance_getter=*/{},
                                          process_memory_budget_bytes,
                                          moderate_budget_ratio,
                                          critical_budget_ratio,
                                          moderate_system_fraction,
                                          critical_system_fraction,
                                          poll_interval,
                                          cooldown) {}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
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
    base::TimeDelta cooldown)
    : ::memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      process_memory_info_getter_(std::move(process_memory_info_getter)),
      system_memory_info_getter_(std::move(system_memory_info_getter)),
      media_allowance_getter_(std::move(media_allowance_getter)),
      process_memory_budget_bytes_(
          process_memory_budget_bytes > 0
              ? process_memory_budget_bytes
              : ResolveProcessMemoryBudget(system_memory_info_getter_)),
      moderate_budget_ratio_(moderate_budget_ratio),
      critical_budget_ratio_(critical_budget_ratio),
      moderate_system_fraction_(moderate_system_fraction),
      critical_system_fraction_(critical_system_fraction),
      poll_interval_(poll_interval),
      cooldown_(cooldown) {
  DCHECK_GE(critical_budget_ratio_, moderate_budget_ratio_);
  DCHECK_GE(moderate_system_fraction_, critical_system_fraction_);

  if (!process_memory_info_getter_) {
    process_metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
    process_memory_info_getter_ = base::BindRepeating(
        [](base::ProcessMetrics* pm) { return pm->GetMemoryInfo(); },
        base::Unretained(process_metrics_.get()));
  }

  Start();
}

CobaltSystemMemoryPressureEvaluator::~CobaltSystemMemoryPressureEvaluator() {
  Stop();
}

void CobaltSystemMemoryPressureEvaluator::Start() {
  CheckMemoryPressure();
  timer_.Start(FROM_HERE, poll_interval_,
               base::BindRepeating(
                   &CobaltSystemMemoryPressureEvaluator::CheckMemoryPressure,
                   weak_ptr_factory_.GetWeakPtr()));
}

void CobaltSystemMemoryPressureEvaluator::Stop() {
  timer_.Stop();
}

void CobaltSystemMemoryPressureEvaluator::CheckMemoryPressure() {
  UpdateMemoryPressureLevel(CalculateCurrentPressureLevel());
}

base::MemoryPressureListener::MemoryPressureLevel
CobaltSystemMemoryPressureEvaluator::CalculateProcessPressureLevel() {
  uint64_t effective_budget = process_memory_budget_bytes_;
  if (media_allowance_getter_) {
    effective_budget += media_allowance_getter_.Run();
  }

  if (effective_budget == 0 || !process_memory_info_getter_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  auto maybe_info = process_memory_info_getter_.Run();
  if (!maybe_info.has_value()) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const auto& info = maybe_info.value();
  uint64_t private_bytes = 0;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
  private_bytes = info.rss_anon_bytes;
#endif
  if (private_bytes == 0) {
    private_bytes = info.resident_set_bytes;
  }

  if (private_bytes == 0) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const float ratio =
      static_cast<float>(private_bytes) / static_cast<float>(effective_budget);

  if (ratio >= critical_budget_ratio_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (ratio >= moderate_budget_ratio_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

base::MemoryPressureListener::MemoryPressureLevel
CobaltSystemMemoryPressureEvaluator::CalculateSystemPressureLevel() {
  if (!system_memory_info_getter_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  base::SystemMemoryInfoKB info;
  if (!system_memory_info_getter_.Run(&info) || info.total <= 0) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const int total_available = (info.available != 0)
                                  ? info.available
                                  : (info.free + info.buffers + info.cached);
  const float ratio =
      static_cast<float>(total_available) / static_cast<float>(info.total);

  if (ratio < critical_system_fraction_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (ratio < moderate_system_fraction_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

base::MemoryPressureListener::MemoryPressureLevel
CobaltSystemMemoryPressureEvaluator::CalculateCurrentPressureLevel() {
  base::MemoryPressureListener::MemoryPressureLevel process_level =
      CalculateProcessPressureLevel();
  base::MemoryPressureListener::MemoryPressureLevel system_level =
      CalculateSystemPressureLevel();
  return std::max(process_level, system_level);
}

void CobaltSystemMemoryPressureEvaluator::UpdateMemoryPressureLevel(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  auto old_vote = current_vote();
  SetCurrentVote(new_level);

  bool notify = false;
  switch (current_vote()) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      repeat_count_ = 0;
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL: {
      if (old_vote != current_vote()) {
        repeat_count_ = 0;
        notify = true;
      } else {
        const int cooldown_cycles =
            std::max(1, static_cast<int>(cooldown_ / poll_interval_));
        if (++repeat_count_ >= cooldown_cycles) {
          repeat_count_ = 0;
          notify = true;
        }
      }
      break;
    }
  }

  SendCurrentVote(notify);
}

}  // namespace memory
}  // namespace cobalt
