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

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "cobalt/browser/features.h"

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/system.h"  // nogncheck
#endif

namespace cobalt {
namespace memory {

namespace {

// Resolves a float fraction parameter from a Finch feature param (which
// includes its default value).
float ResolveFractionParam(const base::FeatureParam<double>& finch_param) {
  double finch_val = finch_param.Get();
  if (finch_val >= 0.0 && finch_val <= 1.0) {
    return static_cast<float>(finch_val);
  }
  return static_cast<float>(finch_param.default_value);
}

// Resolves a TimeDelta parameter from a Finch feature param (in seconds),
// falling back to |default_value| if non-positive.
base::TimeDelta ResolveTimeDeltaParam(
    const base::FeatureParam<int>& finch_param_seconds,
    base::TimeDelta default_value) {
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
  // 1. Finch Feature Param: kCobaltMemoryPressureBudgetMBParam (Highest
  // priority)
  // 2. Hardware RAM Tiering: Derived from Improved Budget Formula (Baseline
  // fallback)
  if (features::kCobaltMemoryPressureBudgetMBParam.Get() > 0) {
    return static_cast<uint64_t>(
               features::kCobaltMemoryPressureBudgetMBParam.Get()) *
           1024 * 1024;
  }

  if (total_physical_memory_bytes == 0) {
#if BUILDFLAG(IS_STARBOARD)
    int64_t total_cpu_memory = SbSystemGetTotalCPUMemory();
    if (total_cpu_memory > 0) {
      total_physical_memory_bytes = static_cast<uint64_t>(total_cpu_memory);
    }
#endif
    if (total_physical_memory_bytes == 0) {
      base::SystemMemoryInfoKB mem_info;
      if (base::GetSystemMemoryInfo(&mem_info)) {
        total_physical_memory_bytes =
            static_cast<uint64_t>(mem_info.total) * 1024;
      }
    }
    LOG(INFO) << "CobaltSystemMemoryPressureEvaluator: Resolved total physical "
              << "RAM: " << (total_physical_memory_bytes / (1024 * 1024))
              << " MB (" << total_physical_memory_bytes << " bytes).";
  }

  // TODO(b/570097834): Align the device tiering logics with DICE.
  // Tier-based budget derivation adhering to the Improved Budget Formula Design
  // (cobalt/tools/performance/memory/improved_memory_budget_formula_design.md).
  // Formula: Process Memory Budget = Device Memory Ceiling - Platform Overhead
  // - Safety Margin

  uint64_t budget = 0;
   if (total_physical_memory_bytes > 0 &&
             total_physical_memory_bytes <= 1024ULL * 1024 * 1024) {
    // Tier 1: Low-End Hardware (> 512 MB and <= 1024 MB physical RAM)
    // Target: 1 GB RDK set-top boxes, 1 GB Android TV dongles.
    // Device Memory Ceiling: 400 MB (Phase 2 Bonsai target; Phase 1 reclaimer
    // ceiling is 450 MB). Platform Overhead: 210 MB (WPEFramework, kernel slab,
    // Mali G31 GPU buffers, Secmem TVP). Safety Margin: 30 MB (absorbs
    // background daemons and OS spikes). Process Budget = 400 MB - 210 MB - 30
    // MB = 160 MB.
    budget = static_cast<uint64_t>(kDefaultBudgetMbLowEnd) * 1024 * 1024;
  } else if (total_physical_memory_bytes > 0 &&
             total_physical_memory_bytes <= 2048ULL * 1024 * 1024) {
    // Tier 2: Standard Hardware (> 1024 MB and <= 2048 MB physical RAM)
    // Target: Mid-tier Smart TVs, Android TV retail boxes (e.g. Chromecast with
    // Google TV HD/4K). Device Memory Ceiling: 750 MB (AOSP foreground
    // application allocation envelope). Platform Overhead: 350 MB (Android
    // system_server, SurfaceFlinger HWC, ART runtime, HALs). Safety Margin: 100
    // MB. Process Budget = 750 MB - 350 MB - 100 MB = 300 MB.
    budget = static_cast<uint64_t>(kDefaultBudgetMbStandard) * 1024 * 1024;
  } else {
    // Tier 3: High-End Hardware (> 2048 MB physical RAM)
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
              features::kCobaltMemoryPressureModerateFractionParam),
          ResolveFractionParam(
              features::kCobaltMemoryPressureCriticalFractionParam),
          ResolveTimeDeltaParam(
              features::kCobaltMemoryPressurePollIntervalSecondsParam,
              kDefaultPollInterval)) {}

CobaltSystemMemoryPressureEvaluator::CobaltSystemMemoryPressureEvaluator(
    std::unique_ptr<::memory_pressure::MemoryPressureVoter> voter,
    ProcessMemoryInfoGetter process_memory_info_getter,
    MediaAllowanceGetter media_allowance_getter,
    uint64_t process_memory_budget_bytes,
    float moderate_process_memory_fraction,
    float critical_process_memory_fraction,
    base::TimeDelta poll_interval)
    : ::memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)),
      process_memory_info_getter_(std::move(process_memory_info_getter)),
      media_allowance_getter_(std::move(media_allowance_getter)),
      process_memory_budget_bytes_(process_memory_budget_bytes > 0
                                       ? process_memory_budget_bytes
                                       : ResolveProcessMemoryBudget()),
      moderate_process_memory_fraction_(moderate_process_memory_fraction),
      critical_process_memory_fraction_(critical_process_memory_fraction),
      poll_interval_(poll_interval > base::TimeDelta() ? poll_interval
                                                       : kDefaultPollInterval) {
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

base::MemoryPressureListener::MemoryPressureLevel
CobaltSystemMemoryPressureEvaluator::CalculateCurrentMemoryPressureLevel() {
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
  uint64_t private_bytes = info.rss_anon_bytes + info.vm_swap_bytes;
  if (private_bytes == 0) {
    private_bytes = info.resident_set_bytes;
  }

  if (private_bytes == 0) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  }

  const float ratio =
      static_cast<float>(private_bytes) / static_cast<float>(effective_budget);

  if (ratio >= critical_process_memory_fraction_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
  if (ratio >= moderate_process_memory_fraction_) {
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

void CobaltSystemMemoryPressureEvaluator::UpdateMemoryPressureLevel(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  SetCurrentVote(new_level);
  bool notify = current_vote() !=
                base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
  SendCurrentVote(notify);
}

}  // namespace memory
}  // namespace cobalt
