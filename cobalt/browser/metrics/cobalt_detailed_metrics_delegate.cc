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

bool MatchStartsWith(absl::string_view name, absl::string_view pattern) {
  return absl::StartsWith(name, pattern);
}

bool MatchContains(absl::string_view name, absl::string_view pattern) {
  return absl::StrContains(name, pattern);
}

bool MatchEndsWith(absl::string_view name, absl::string_view pattern) {
  return absl::EndsWith(name, pattern);
}

struct CategoryPattern {
  const char* label;
  const char* pattern;
  bool (*match_func)(absl::string_view, absl::string_view);
};

// Ordered by probability/impact for early exit optimization.
static constexpr CategoryPattern kCobaltPatterns[] = {
    {"v8", "[anon:v8]", MatchStartsWith},
    {"partition_alloc", "[anon:partition_alloc]", MatchStartsWith},
    {"lib_chrobalt", "libchrobalt", MatchContains},
    {"lib_chrobalt", "libcobalt", MatchContains},
    {"lib_chrobalt", "/cobalt", MatchEndsWith},
    {"code_other", ".so", MatchContains},
    {"code_other", ".apk", MatchContains},
    {"code_other", ".dex", MatchContains},
    {"graphics", "/dev/kgsl-3d0", MatchContains},
    {"graphics", "/dev/mali0", MatchContains},
    {"graphics", "/dev/nvgpu", MatchContains},
    {"graphics", "/dev/pvr", MatchContains},
    {"graphics", "/dev/dri/", MatchContains},
    {"fonts", "/system/fonts/", MatchStartsWith},
    {"fonts", ".ttf", MatchContains},
    {"fonts", ".ttc", MatchContains},
    {"malloc", "[anon:scudo]", MatchStartsWith},
    {"malloc", "[heap]", MatchStartsWith},
    {"malloc", "jemalloc", MatchContains},
    {"android_runtime", "[anon:dalvik-", MatchStartsWith},
    {"android_runtime", ".art", MatchContains},
    {"android_runtime", ".oat", MatchContains},
    {"android_runtime", ".vdex", MatchContains},
    {"android_runtime", ".odex", MatchContains},
    {"ashmem_jit", "/dev/ashmem/", MatchContains},
    {"ashmem_jit", "memfd:jit", MatchContains},
    {"stacks", "[stack]", MatchStartsWith},
    {"stacks", "stack_and_tls", MatchContains},
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
      if (cp.match_func(name, cp.pattern)) {
        label = cp.label;
        break;
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
