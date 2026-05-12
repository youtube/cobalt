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

#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/strings/match.h"

namespace cobalt {

namespace {

// Ordered by probability/impact for early exit optimization.
static constexpr CategoryPattern kCobaltPatterns[] = {
    {"v8", "[anon:v8]", /*is_prefix=*/true},
    {"partition_alloc", "[anon:partition_alloc]", /*is_prefix=*/true},
    {"lib_chrobalt", "libchrobalt.so", /*is_prefix=*/false},
    {"cobalt_core", "libcobalt.so", /*is_prefix=*/false},
    {"graphics", "/dev/kgsl-3d0", /*is_prefix=*/false},
    {"graphics", "/dev/mali0", /*is_prefix=*/false},
    {"graphics", "/dev/nvgpu", /*is_prefix=*/false},
    {"graphics", "/dev/pvr", /*is_prefix=*/false},
    {"graphics", "/dev/dri/", /*is_prefix=*/false},
    {"fonts", "/system/fonts/", /*is_prefix=*/true},
    {"fonts", ".ttf", /*is_prefix=*/false},
    {"fonts", ".ttc", /*is_prefix=*/false},
    {"malloc", "[anon:scudo]", /*is_prefix=*/true},
    {"malloc", "[heap]", /*is_prefix=*/true},
    {"malloc", "jemalloc", /*is_prefix=*/false},
    {"android_runtime", "[anon:dalvik-", /*is_prefix=*/true},
    {"android_runtime", ".art", /*is_prefix=*/false},
    {"android_runtime", ".oat", /*is_prefix=*/false},
    {"android_runtime", ".vdex", /*is_prefix=*/false},
    {"android_runtime", ".odex", /*is_prefix=*/false},
    {"ashmem_jit", "/dev/ashmem/", /*is_prefix=*/false},
    {"ashmem_jit", "memfd:jit", /*is_prefix=*/false},
    {"stacks", "[stack]", /*is_prefix=*/true},
    {"stacks", "stack_and_tls", /*is_prefix=*/false},
};

}  // namespace

CobaltDetailedMetricsDelegate::CobaltDetailedMetricsDelegate() = default;
CobaltDetailedMetricsDelegate::~CobaltDetailedMetricsDelegate() = default;

void CobaltDetailedMetricsDelegate::OnSmapsEntry(
    absl::string_view name,
    const memory_instrumentation::SmapsMetrics& metrics) {
  absl::string_view label = "other";

  if (name.empty()) {
    label = "anonymous_other";
  } else {
    for (const auto& cp : kCobaltPatterns) {
      if (cp.is_prefix) {
        if (absl::StartsWith(name, cp.pattern)) {
          label = cp.label;
          break;
        }
      } else {
        if (absl::StrContains(name, cp.pattern)) {
          label = cp.label;
          break;
        }
      }
    }
  }

  base::AutoLock lock(lock_);
  accumulated_stats_[base::StrCat({"pss:", label})] += metrics.pss_kb;
  accumulated_stats_[base::StrCat({"rss:", label})] += metrics.rss_kb;
  accumulated_stats_["total_pss"] += metrics.pss_kb;
  accumulated_stats_["total_rss"] += metrics.rss_kb;
}

void CobaltDetailedMetricsDelegate::GetAndResetStats(
    base::flat_map<std::string, uint64_t>* stats) {
  base::AutoLock lock(lock_);
  stats->swap(accumulated_stats_);
  accumulated_stats_.clear();
}

base::WeakPtr<memory_instrumentation::DetailedMetricsDelegate>
CobaltDetailedMetricsDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace cobalt
