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

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace cobalt {

namespace {

// Pattern constants for categorization.
constexpr char kLibCobaltPattern[] = "libcobalt.so";
constexpr char kLibChrobaltPattern[] = "libchrobalt.so";
constexpr char kCobaltCorePattern[] = "CobaltCore";
constexpr char kV8Pattern[] = "v8";
constexpr char kPartitionAllocPattern[] = "partition_alloc";
constexpr char kScudoPattern[] = "[anon:scudo]";
constexpr char kHeapPattern[] = "[heap]";
constexpr char kJemallocPattern[] = "jemalloc";
constexpr char kDalvikPrefix[] = "[anon:dalvik-";
constexpr char kArtExtension[] = ".art";
constexpr char kOatExtension[] = ".oat";
constexpr char kVdexExtension[] = ".vdex";
constexpr char kOdexExtension[] = ".odex";
constexpr char kKgslPattern[] = "/dev/kgsl-3d0";
constexpr char kMaliPattern[] = "/dev/mali0";
constexpr char kNvgpuPattern[] = "/dev/nvgpu";
constexpr char kPvrPattern[] = "/dev/pvr";
constexpr char kDriPattern[] = "/dev/dri/";
constexpr char kTtfExtension[] = ".ttf";
constexpr char kTtcExtension[] = ".ttc";
constexpr char kFontsPath[] = "fonts/";
constexpr char kAshmemPath[] = "/dev/ashmem/";
constexpr char kMemfdJitPattern[] = "memfd:jit";
constexpr char kStackPattern[] = "[stack]";
constexpr char kStackAndTlsPattern[] = "stack_and_tls";
constexpr char kSoExtension[] = ".so";
constexpr char kApkExtension[] = ".apk";
constexpr char kDexExtension[] = ".dex";
constexpr char kAnonPrefix[] = "[anon:";

}  // namespace

CobaltDetailedMetricsDelegate::CobaltDetailedMetricsDelegate() = default;
CobaltDetailedMetricsDelegate::~CobaltDetailedMetricsDelegate() = default;

bool CobaltDetailedMetricsDelegate::OnSmapsBuffer(std::string_view line) {
  if (line.empty()) {
    return true;
  }

  // Check if it's a header line (starts with hex address range).
  if (base::IsHexDigit(static_cast<unsigned char>(line[0]))) {
    current_category_ = GetCategory(line);
    return true;
  }

  if (current_category_ == Category::kNone) {
    return true;
  }

  // Parse counter line. Format: "Name:  Value kB"
  bool is_pss = base::StartsWith(line, "Pss:");
  bool is_rss = base::StartsWith(line, "Rss:");

  if (!is_pss && !is_rss) {
    return true;
  }

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Expected tokens: ["Pss:", "12", "kB"] or ["Rss:", "12", "kB"]
  if (tokens.size() < 3 || tokens.back() != "kB") {
    return true;
  }

  uint64_t value_kb = 0;
  if (!base::StringToUint64(tokens[1], &value_kb)) {
    return true;
  }

  Stats* stats = nullptr;
  switch (current_category_) {
    case Category::kCobaltCore:
      stats = &cobalt_core_;
      break;
    case Category::kLibChrobalt:
      stats = &lib_chrobalt_;
      break;
    case Category::kV8:
      stats = &v8_;
      break;
    case Category::kPartitionAlloc:
      stats = &partition_alloc_;
      break;
    case Category::kMalloc:
      stats = &malloc_;
      break;
    case Category::kAndroidRuntime:
      stats = &android_runtime_;
      break;
    case Category::kGraphics:
      stats = &graphics_;
      break;
    case Category::kFonts:
      stats = &fonts_;
      break;
    case Category::kAshmemJit:
      stats = &ashmem_jit_;
      break;
    case Category::kStacks:
      stats = &stacks_;
      break;
    case Category::kCodeOther:
      stats = &code_other_;
      break;
    case Category::kAnonymousOther:
      stats = &anonymous_other_;
      break;
    case Category::kOther:
      stats = &other_;
      break;
    default:
      break;
  }

  if (stats) {
    if (is_pss) {
      stats->pss_kb += value_kb;
    } else {
      stats->rss_kb += value_kb;
    }
  }
  return true;
}

memory_instrumentation::DetailedMetrics
CobaltDetailedMetricsDelegate::GetAndResetStats() {
  memory_instrumentation::DetailedMetrics result;

  uint64_t total_pss = 0;
  uint64_t total_rss = 0;

  auto add_stats = [&](const std::string& name, Stats& stats) {
    result.categories_kb["pss:" + name] =
        base::saturated_cast<uint32_t>(stats.pss_kb);
    result.categories_kb["rss:" + name] =
        base::saturated_cast<uint32_t>(stats.rss_kb);
    total_pss += stats.pss_kb;
    total_rss += stats.rss_kb;
    stats.pss_kb = 0;
    stats.rss_kb = 0;
  };

  add_stats("cobalt_core", cobalt_core_);
  add_stats("lib_chrobalt", lib_chrobalt_);
  add_stats("v8", v8_);
  add_stats("partition_alloc", partition_alloc_);
  add_stats("malloc", malloc_);
  add_stats("android_runtime", android_runtime_);
  add_stats("graphics", graphics_);
  add_stats("fonts", fonts_);
  add_stats("ashmem_jit", ashmem_jit_);
  add_stats("stacks", stacks_);
  add_stats("code_other", code_other_);
  add_stats("anonymous_other", anonymous_other_);
  add_stats("other", other_);

  result.total_pss_kb = base::saturated_cast<uint32_t>(total_pss);
  result.total_rss_kb = base::saturated_cast<uint32_t>(total_rss);

  current_category_ = Category::kNone;
  return result;
}

CobaltDetailedMetricsDelegate::Category
CobaltDetailedMetricsDelegate::GetCategory(std::string_view line) {
  // Group by common sub-patterns to reduce searches.

  // Optimization: Check for anonymous mappings.
  if (base::Contains(line, kAnonPrefix)) {
    if (base::Contains(line, kV8Pattern)) {
      return Category::kV8;
    }
    if (base::Contains(line, kPartitionAllocPattern)) {
      return Category::kPartitionAlloc;
    }
    if (base::Contains(line, kScudoPattern)) {
      return Category::kMalloc;
    }
    if (base::Contains(line, kDalvikPrefix)) {
      return Category::kAndroidRuntime;
    }
    return Category::kAnonymousOther;
  }

  // Optimization: Check for common shared library extension.
  if (base::Contains(line, kSoExtension)) {
    if (base::Contains(line, kLibChrobaltPattern)) {
      return Category::kLibChrobalt;
    }
    if (base::Contains(line, kLibCobaltPattern)) {
      return Category::kCobaltCore;
    }
    return Category::kCodeOther;
  }

  // Optimization: Check for device mappings (graphics and ashmem).
  if (base::Contains(line, "/dev/")) {
    if (base::Contains(line, kKgslPattern) ||
        base::Contains(line, kMaliPattern) ||
        base::Contains(line, kNvgpuPattern) ||
        base::Contains(line, kPvrPattern) ||
        base::Contains(line, kDriPattern)) {
      return Category::kGraphics;
    }
    if (base::Contains(line, kAshmemPath)) {
      return Category::kAshmemJit;
    }
  }

  // Precedence 1: cobalt_core (additional patterns)
  if (base::Contains(line, kCobaltCorePattern)) {
    return Category::kCobaltCore;
  }

  // Precedence 2: malloc (heap)
  if (base::Contains(line, kHeapPattern) ||
      base::Contains(line, kJemallocPattern)) {
    return Category::kMalloc;
  }

  // Precedence 3: android_runtime (additional extensions)
  if (base::Contains(line, kArtExtension) ||
      base::Contains(line, kOatExtension) ||
      base::Contains(line, kVdexExtension) ||
      base::Contains(line, kOdexExtension)) {
    return Category::kAndroidRuntime;
  }

  // Precedence 5: fonts
  if (base::Contains(line, kTtfExtension) ||
      base::Contains(line, kTtcExtension) || base::Contains(line, kFontsPath)) {
    return Category::kFonts;
  }

  // Precedence 6: ashmem_jit (additional patterns)
  if (base::Contains(line, kMemfdJitPattern)) {
    return Category::kAshmemJit;
  }

  // Precedence 7: stacks
  if (base::Contains(line, kStackPattern) ||
      base::Contains(line, kStackAndTlsPattern)) {
    return Category::kStacks;
  }

  // Precedence 8: code_other (additional extensions)
  if (base::Contains(line, kApkExtension) ||
      base::Contains(line, kDexExtension)) {
    return Category::kCodeOther;
  }

  // Precedence 10: other
  return Category::kOther;
}

}  // namespace cobalt
