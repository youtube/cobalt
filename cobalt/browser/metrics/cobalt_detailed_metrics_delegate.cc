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

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/strings/match.h"
#include "third_party/abseil-cpp/absl/strings/numbers.h"

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

void CobaltDetailedMetricsDelegate::OnSmapsLine(absl::string_view line) {
  if (line.empty()) {
    return;
  }

  // Check if it's a header line (starts with hex address range).
  if (base::IsHexDigit(static_cast<unsigned char>(line[0]))) {
    current_category_ = GetCategory(line);
    return;
  }

  if (current_category_ == Category::kNone) {
    return;
  }

  // Parse counter line. Format: "Name:  Value kB"
  bool is_pss = absl::StartsWith(line, "Pss:");
  bool is_rss = absl::StartsWith(line, "Rss:");

  if (!is_pss && !is_rss) {
    return;
  }

  std::vector<absl::string_view> tokens = base::SplitStringPiece(
      line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Expected tokens: ["Pss:", "12", "kB"] or ["Rss:", "12", "kB"]
  if (tokens.size() < 3 || tokens.back() != "kB") {
    return;
  }

  uint64_t value_kb = 0;
  if (!absl::SimpleAtoi(tokens[1], &value_kb)) {
    return;
  }

  Stats* stats = nullptr;
  switch (current_category_) {
    case Category::kCobaltCore:
      stats = &cobalt_core_;
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
}

base::flat_map<std::string, uint32_t>
CobaltDetailedMetricsDelegate::GetAndResetStats() {
  base::flat_map<std::string, uint32_t> result;

  auto add_stats = [&](const std::string& name, Stats& stats) {
    result["pss:" + name] = base::saturated_cast<uint32_t>(stats.pss_kb);
    result["rss:" + name] = base::saturated_cast<uint32_t>(stats.rss_kb);
    stats.pss_kb = 0;
    stats.rss_kb = 0;
  };

  add_stats("cobalt_core", cobalt_core_);
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

  current_category_ = Category::kNone;
  return result;
}

CobaltDetailedMetricsDelegate::Category
CobaltDetailedMetricsDelegate::GetCategory(absl::string_view line) {
  // Optimized check for anonymous mappings.
  if (absl::StrContains(line, kAnonPrefix)) {
    if (absl::StrContains(line, kV8Pattern)) {
      return Category::kV8;
    }
    if (absl::StrContains(line, kPartitionAllocPattern)) {
      return Category::kPartitionAlloc;
    }
    if (absl::StrContains(line, kScudoPattern)) {
      return Category::kMalloc;
    }
    if (absl::StrContains(line, kDalvikPrefix)) {
      return Category::kAndroidRuntime;
    }
    return Category::kAnonymousOther;
  }

  // Optimized check for common file-backed mappings.
  if (absl::StrContains(line, kSoExtension)) {
    if (absl::StrContains(line, kLibCobaltPattern) ||
        absl::StrContains(line, kLibChrobaltPattern)) {
      return Category::kCobaltCore;
    }
    return Category::kCodeOther;
  }

  // Precedence 1: cobalt_core (additional patterns)
  if (absl::StrContains(line, kCobaltCorePattern)) {
    return Category::kCobaltCore;
  }

  // Precedence 2: malloc (heap)
  if (absl::StrContains(line, kHeapPattern) ||
      absl::StrContains(line, kJemallocPattern)) {
    return Category::kMalloc;
  }

  // Precedence 3: android_runtime (additional extensions)
  if (absl::StrContains(line, kArtExtension) ||
      absl::StrContains(line, kOatExtension) ||
      absl::StrContains(line, kVdexExtension) ||
      absl::StrContains(line, kOdexExtension)) {
    return Category::kAndroidRuntime;
  }

  // Precedence 4: graphics
  if (absl::StrContains(line, kKgslPattern) ||
      absl::StrContains(line, kMaliPattern) ||
      absl::StrContains(line, kNvgpuPattern) ||
      absl::StrContains(line, kPvrPattern) ||
      absl::StrContains(line, kDriPattern)) {
    return Category::kGraphics;
  }

  // Precedence 5: fonts
  if (absl::StrContains(line, kTtfExtension) ||
      absl::StrContains(line, kTtcExtension) ||
      absl::StrContains(line, kFontsPath)) {
    return Category::kFonts;
  }

  // Precedence 6: ashmem_jit
  if (absl::StrContains(line, kAshmemPath) ||
      absl::StrContains(line, kMemfdJitPattern)) {
    return Category::kAshmemJit;
  }

  // Precedence 7: stacks
  if (absl::StrContains(line, kStackPattern) ||
      absl::StrContains(line, kStackAndTlsPattern)) {
    return Category::kStacks;
  }

  // Precedence 8: code_other (additional extensions)
  if (absl::StrContains(line, kApkExtension) ||
      absl::StrContains(line, kDexExtension)) {
    return Category::kCodeOther;
  }

  // Precedence 10: other
  return Category::kOther;
}

}  // namespace cobalt
