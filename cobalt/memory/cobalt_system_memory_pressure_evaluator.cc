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
#include "base/system/sys_info.h"
#include "cobalt/browser/features.h"
#include "starboard/system.h"

namespace cobalt {
namespace memory {

namespace {

// Parameter Resolution Priority Order:
// 1. Command-Line Switches (Highest priority: overrides all for local debugging
// & QA scripts)
// 2. Finch Feature Parameters (Middle priority: server-driven A/B testing &
// field studies)
// 3. Constexpr Defaults / Hardware Tiering (Baseline fallback)

constexpr char kSwitchModerateProcessMemoryFraction[] =
    "cobalt-memory-pressure-moderate-process-memory-fraction";
constexpr char kSwitchCriticalProcessMemoryFraction[] =
    "cobalt-memory-pressure-critical-process-memory-fraction";
constexpr char kSwitchPollIntervalMs[] =
    "cobalt-memory-pressure-poll-interval-ms";
constexpr char kSwitchCooldownMs[] = "cobalt-memory-pressure-cooldown-ms";

// Resolves a float fraction parameter following the priority order:
// CLI switch -> Finch feature param (which includes default value).
float ResolveFractionParam(const char* switch_name,
                           const base::FeatureParam<double>& finch_param) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(switch_name)) {
    double value;
    if (base::StringToDouble(command_line->GetSwitchValueASCII(switch_name),
                             &value) &&
        value >= 0.0 && value <= 1.0) {
      return static_cast<float>(value);
    }
  }
  double finch_val = finch_param.Get();
  if (finch_val >= 0.0 && finch_val <= 1.0) {
    return static_cast<float>(finch_val);
  }
  return static_cast<float>(finch_param.default_value);
}

// Resolves a TimeDelta parameter following the priority order:
// CLI switch (in ms) -> Finch feature param (in seconds) -> fallback default.
base::TimeDelta ResolveTimeDeltaParam(
    const char* switch_name,
    const base::FeatureParam<int>& finch_param_seconds,
    base::TimeDelta default_value) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line && command_line->HasSwitch(switch_name)) {
    int value;
    if (base::StringToInt(command_line->GetSwitchValueASCII(switch_name),
                          &value) &&
        value > 0) {
      return base::Milliseconds(value);
    }
  }
  int finch_seconds = finch_param_seconds.Get();
  if (finch_seconds > 0) {
    return base::Seconds(finch_seconds);
  }
  return default_value;
}

}  // namespace

// static
uint64_t CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
    uint64_t total_physical_memory_bytes) {
  // Budget Resolution Priority Order:
  // 1. Command-Line Switch: --process-memory-budget-mb (Highest priority)
  // 2. Finch Feature Param: kCobaltMemoryPressureBudgetMBParam (Middle
  // priority)
  // 3. Hardware RAM Tiering: Derived from Improved Budget Formula (Baseline
  // fallback)
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

  if (features::kCobaltMemoryPressureBudgetMBParam.Get() > 0) {
    return static_cast<uint64_t>(
               features::kCobaltMemoryPressureBudgetMBParam.Get()) *
           1024 * 1024;
  }

  if (total_physical_memory_bytes == 0) {
    int64_t total_cpu_memory = SbSystemGetTotalCPUMemory();
    if (total_cpu_memory > 0) {
      total_physical_memory_bytes = static_cast<uint64_t>(total_cpu_memory);
    } else {
      total_physical_memory_bytes =
          base::SysInfo::AmountOfPhysicalMemory().InBytesUnsigned();
    }
    LOG(INFO) << "CobaltSystemMemoryPressureEvaluator: Resolved total physical "
              << "RAM: " << (total_physical_memory_bytes / (1024 * 1024))
              << " MB via SbSystemGetTotalCPUMemory() ("
              << total_physical_memory_bytes << " bytes).";
  }

  // TODO(b/570097834): Align the device tiering logics with DICE.
  // Tier-based budget derivation adhering to the Improved Budget Formula Design
  // (cobalt/tools/performance/memory/improved_memory_budget_formula_design.md).
  // Formula: Process Memory Budget = Device Memory Ceiling - Platform Overhead
  // - Safety Margin

  uint64_t budget = 0;
  // Tier 1: Super Low-End Hardware (<= 512 MB physical RAM)
  // Target: 512 MB connected TVs, streaming sticks (e.g. Roku 512 MB boards).
  // Device Memory Ceiling: 271 MB (strict kernel LMK kill threshold combining
  // CPU+GPU). Platform Overhead: 125 MB (kernel base, display compositor
  // planes, OEM daemons). Safety Margin: 26 MB (10% cushion). Process Budget =
  // 271 MB - 125 MB - 26 MB = 120 MB.
  if (total_physical_memory_bytes > 0 &&
      total_physical_memory_bytes <= 512ULL * 1024 * 1024) {
    budget = static_cast<uint64_t>(kDefaultBudgetMbSuperLowEnd) * 1024 * 1024;
  } else if (total_physical_memory_bytes > 0 &&
             total_physical_memory_bytes <= 1024ULL * 1024 * 1024) {
    // Tier 2: Low-End Hardware (> 512 MB and <= 1024 MB physical RAM)
    // Target: 1 GB RDK set-top boxes, 1 GB Android TV dongles.
    // Device Memory Ceiling: 400 MB (Phase 2 Bonsai target; Phase 1 reclaimer
    // ceiling is 450 MB). Platform Overhead: 210 MB (WPEFramework, kernel slab,
    // Mali G31 GPU buffers, Secmem TVP). Safety Margin: 30 MB (absorbs
    // background daemons and OS spikes). Process Budget = 400 MB - 210 MB - 30
    // MB = 160 MB.
    budget = static_cast<uint64_t>(kDefaultBudgetMbLowEnd) * 1024 * 1024;
  } else if (total_physical_memory_bytes > 0 &&
             total_physical_memory_bytes <= 2048ULL * 1024 * 1024) {
    // Tier 3: Standard Hardware (> 1024 MB and <= 2048 MB physical RAM)
    // Target: Mid-tier Smart TVs, Android TV retail boxes (e.g. Chromecast with
    // Google TV HD/4K). Device Memory Ceiling: 750 MB (AOSP foreground
    // application allocation envelope). Platform Overhead: 350 MB (Android
    // system_server, SurfaceFlinger HWC, ART runtime, HALs). Safety Margin: 100
    // MB. Process Budget = 750 MB - 350 MB - 100 MB = 300 MB.
    budget = static_cast<uint64_t>(kDefaultBudgetMbStandard) * 1024 * 1024;
  } else {
    // Tier 4: High-End Hardware (> 2048 MB physical RAM)
    // Target: Premium Smart TVs, Game Consoles, 3GB+ STBs.
    // Process Budget: 500 MB (fixed ceiling to avoid unbounded JS heap bloat
    // while preventing V8 GC hitches during long browsing sessions).
    budget = static_cast<uint64_t>(kDefaultBudgetMbHighEnd) * 1024 * 1024;
  }

  LOG(INFO) << "CobaltSystemMemoryPressureEvaluator: Derived process memory "
            << "budget = " << (budget / (1024 * 1024)) << " MB.";
  return budget;
}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    MediaAllowanceGetter media_allowance_getter)
    : CobaltSystemMemoryPressureEvaluator(
          std::move(voter),
          /*process_memory_info_getter=*/{},
          std::move(media_allowance_getter),
          /*process_memory_budget_bytes=*/0,
          ResolveFractionParam(
              kSwitchModerateProcessMemoryFraction,
              features::kCobaltMemoryPressureModerateFractionParam),
          ResolveFractionParam(
              kSwitchCriticalProcessMemoryFraction,
              features::kCobaltMemoryPressureCriticalFractionParam),
          ResolveTimeDeltaParam(
              kSwitchPollIntervalMs,
              features::kCobaltMemoryPressurePollIntervalSecondsParam,
              kDefaultPollInterval),
          ResolveTimeDeltaParam(
              kSwitchCooldownMs,
              features::kCobaltMemoryPressureCooldownSecondsParam,
              kDefaultCooldown)) {}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    ProcessMemoryInfoGetter process_memory_info_getter,
    uint64_t process_memory_budget_bytes,
    float moderate_process_memory_fraction,
    float critical_process_memory_fraction,
    base::TimeDelta poll_interval,
    base::TimeDelta cooldown)
    : CobaltSystemMemoryPressureEvaluator(std::move(voter),
                                          std::move(process_memory_info_getter),
                                          /*media_allowance_getter=*/{},
                                          process_memory_budget_bytes,
                                          moderate_process_memory_fraction,
                                          critical_process_memory_fraction,
                                          poll_interval,
                                          cooldown) {}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    ProcessMemoryInfoGetter process_memory_info_getter,
    MediaAllowanceGetter media_allowance_getter,
    uint64_t process_memory_budget_bytes,
    float moderate_process_memory_fraction,
    float critical_process_memory_fraction,
    base::TimeDelta poll_interval,
    base::TimeDelta cooldown)
    : ::memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      process_memory_info_getter_(std::move(process_memory_info_getter)),
      media_allowance_getter_(std::move(media_allowance_getter)),
      process_memory_budget_bytes_(process_memory_budget_bytes > 0
                                       ? process_memory_budget_bytes
                                       : ResolveProcessMemoryBudget()),
      moderate_process_memory_fraction_(moderate_process_memory_fraction),
      critical_process_memory_fraction_(critical_process_memory_fraction),
      poll_interval_(poll_interval > base::TimeDelta() ? poll_interval
                                                       : kDefaultPollInterval),
      cooldown_(cooldown > base::TimeDelta() ? cooldown : kDefaultCooldown) {
  DCHECK_GE(critical_process_memory_fraction_,
            moderate_process_memory_fraction_);
  DCHECK_GE(moderate_process_memory_fraction_, 0.0f);
  DCHECK_LE(critical_process_memory_fraction_, 1.0f);

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
  UpdateMemoryPressureLevel(CalculateCurrentMemoryPressureLevel());
}

base::MemoryPressureLevel
CobaltSystemMemoryPressureEvaluator::CalculateCurrentMemoryPressureLevel() {
  uint64_t effective_budget = process_memory_budget_bytes_;
  if (media_allowance_getter_) {
    effective_budget += media_allowance_getter_.Run();
  }

  if (effective_budget == 0 || !process_memory_info_getter_) {
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  auto maybe_info = process_memory_info_getter_.Run();
  if (!maybe_info.has_value()) {
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const auto& info = maybe_info.value();
  uint64_t private_bytes = 0;
#if BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  private_bytes = info.rss_anon_bytes + info.vm_swap_bytes;
#endif
  if (private_bytes == 0) {
    private_bytes = info.resident_set_bytes;
  }

  if (private_bytes == 0) {
    return base::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const float ratio =
      static_cast<float>(private_bytes) / static_cast<float>(effective_budget);

  if (ratio >= critical_process_memory_fraction_) {
    return base::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (ratio >= moderate_process_memory_fraction_) {
    return base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MEMORY_PRESSURE_LEVEL_NONE;
}

void CobaltSystemMemoryPressureEvaluator::UpdateMemoryPressureLevel(
    base::MemoryPressureLevel new_level) {
  base::MemoryPressureLevel old_vote = current_vote();
  SetCurrentVote(new_level);

  // |notify| will be set to true if MemoryPressureListeners need to be
  // notified of a memory pressure level state change.
  bool notify = false;
  switch (current_vote()) {
    case base::MEMORY_PRESSURE_LEVEL_NONE:
      break;

    case base::MEMORY_PRESSURE_LEVEL_MODERATE:
      if (old_vote != current_vote()) {
        // This is a new transition to moderate pressure so notify.
        moderate_pressure_repeat_count_ = 0;
        notify = true;
      } else {
        // Already in moderate pressure, only notify if sustained over the
        // cooldown period.
        const int moderate_pressure_cooldown_cycles =
            std::max(1, static_cast<int>(cooldown_ / poll_interval_));
        if (++moderate_pressure_repeat_count_ ==
            moderate_pressure_cooldown_cycles) {
          moderate_pressure_repeat_count_ = 0;
          notify = true;
        }
      }
      break;

    case base::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Always notify of critical pressure levels.
      notify = true;
      break;
  }

  SendCurrentVote(notify);
}

}  // namespace memory
}  // namespace cobalt
