// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/metrics/cobalt_os_metrics_delegate.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/singleton.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace cobalt {

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kLibChrobaltPattern[] = "libchrobalt.so";
constexpr char kPartitionAllocPattern[] = "partition_alloc";
constexpr char kV8Pattern[] = "v8";
constexpr char kScudoPattern[] = "scudo";
constexpr char kHeapPattern[] = "[heap]";
constexpr char kSoExtension[] = ".so";
constexpr char kApkExtension[] = ".apk";
constexpr char kDexExtension[] = ".dex";
constexpr char kTtfExtension[] = ".ttf";
constexpr char kTtcExtension[] = ".ttc";
constexpr char kFontsPath[] = "fonts/";
constexpr char kAshmemPath[] = "/dev/ashmem/";
constexpr char kMemfdJitPattern[] = "memfd:jit";
constexpr char kArtExtension[] = ".art";
constexpr char kOatExtension[] = ".oat";
constexpr char kVdexExtension[] = ".vdex";
constexpr char kOdexExtension[] = ".odex";
constexpr char kJarExtension[] = ".jar";
constexpr char kHybExtension[] = ".hyb";
constexpr char kDalvikPrefix[] = "dalvik-";
constexpr char kStackAndTlsPattern[] = "stack_and_tls";
constexpr char kStackPattern[] = "[stack]";
#endif

}  // namespace

const char kExtraStatLibChrobaltPss[] = "LibChrobaltPss";
const char kExtraStatLibChrobaltRss[] = "LibChrobaltRss";
const char kExtraStatPartitionAllocRss[] = "PartitionAllocRss";
const char kExtraStatV8Rss[] = "V8Rss";
const char kExtraStatMallocRss[] = "MallocRss";
const char kExtraStatCodeOtherRss[] = "CodeOtherRss";
const char kExtraStatFontsRss[] = "FontsRss";
const char kExtraStatAshmemJitRss[] = "AshmemJitRss";
const char kExtraStatAndroidRuntimeRss[] = "AndroidRuntimeRss";
const char kExtraStatStacksRss[] = "StacksRss";

// static
CobaltOSMetricsDelegate* CobaltOSMetricsDelegate::GetInstance() {
  return base::Singleton<CobaltOSMetricsDelegate>::get();
}

CobaltOSMetricsDelegate::CobaltOSMetricsDelegate() = default;
CobaltOSMetricsDelegate::~CobaltOSMetricsDelegate() = default;

void CobaltOSMetricsDelegate::OnSmapsHeader(const char* line) {
#if BUILDFLAG(IS_ANDROID)
  std::string_view line_sp(line);
  if (base::Contains(line_sp, kLibChrobaltPattern)) {
    current_type_ = RegionType::kLibChrobalt;
  } else if (base::Contains(line_sp, kPartitionAllocPattern)) {
    current_type_ = RegionType::kPartitionAlloc;
  } else if (base::Contains(line_sp, kV8Pattern)) {
    current_type_ = RegionType::kV8;
  } else if (base::Contains(line_sp, kScudoPattern) ||
             base::Contains(line_sp, kHeapPattern)) {
    current_type_ = RegionType::kMalloc;
  } else if (base::Contains(line_sp, kTtfExtension) ||
             base::Contains(line_sp, kTtcExtension) ||
             base::Contains(line_sp, kFontsPath)) {
    current_type_ = RegionType::kFonts;
  } else if (base::Contains(line_sp, kAshmemPath) ||
             base::Contains(line_sp, kMemfdJitPattern)) {
    current_type_ = RegionType::kAshmemJit;
  } else if (base::Contains(line_sp, kArtExtension) ||
             base::Contains(line_sp, kOatExtension) ||
             base::Contains(line_sp, kVdexExtension) ||
             base::Contains(line_sp, kOdexExtension) ||
             base::Contains(line_sp, kJarExtension) ||
             base::Contains(line_sp, kHybExtension) ||
             base::Contains(line_sp, kDalvikPrefix)) {
    current_type_ = RegionType::kAndroidRuntime;
  } else if (base::Contains(line_sp, kStackAndTlsPattern) ||
             base::Contains(line_sp, kStackPattern)) {
    current_type_ = RegionType::kStacks;
  } else if (base::Contains(line_sp, kSoExtension) ||
             base::Contains(line_sp, kApkExtension) ||
             base::Contains(line_sp, kDexExtension)) {
    current_type_ = RegionType::kCodeOther;
  } else {
    current_type_ = RegionType::kNone;
  }
#endif
}

void CobaltOSMetricsDelegate::OnSmapsCounter(
    const char* name,
    uint64_t value_kb,
    memory_instrumentation::mojom::RawOSMemDump* dump) {
#if BUILDFLAG(IS_ANDROID)
  if (current_type_ == RegionType::kNone) {
    return;
  }

  std::string_view name_sp(name);
  if (name_sp == "Pss:") {
    if (current_type_ == RegionType::kLibChrobalt) {
      libchrobalt_pss_kb_ += value_kb;
    }
  } else if (name_sp == "Rss:") {
    switch (current_type_) {
      case RegionType::kLibChrobalt:
        libchrobalt_rss_kb_ += value_kb;
        break;
      case RegionType::kPartitionAlloc:
        pa_rss_kb_ += value_kb;
        break;
      case RegionType::kV8:
        v8_rss_kb_ += value_kb;
        break;
      case RegionType::kMalloc:
        malloc_rss_kb_ += value_kb;
        break;
      case RegionType::kCodeOther:
        code_other_rss_kb_ += value_kb;
        break;
      case RegionType::kFonts:
        fonts_rss_kb_ += value_kb;
        break;
      case RegionType::kAshmemJit:
        ashmem_jit_rss_kb_ += value_kb;
        break;
      case RegionType::kAndroidRuntime:
        android_runtime_rss_kb_ += value_kb;
        break;
      case RegionType::kStacks:
        stacks_rss_kb_ += value_kb;
        break;
      default:
        break;
    }
  }
#endif
}

void CobaltOSMetricsDelegate::OnSmapsFinished(
    memory_instrumentation::mojom::RawOSMemDump* dump) {
#if BUILDFLAG(IS_ANDROID)
  dump->extra_stats[kExtraStatLibChrobaltPss] =
      base::saturated_cast<uint32_t>(libchrobalt_pss_kb_);
  dump->extra_stats[kExtraStatLibChrobaltRss] =
      base::saturated_cast<uint32_t>(libchrobalt_rss_kb_);
  dump->extra_stats[kExtraStatPartitionAllocRss] =
      base::saturated_cast<uint32_t>(pa_rss_kb_);
  dump->extra_stats[kExtraStatV8Rss] =
      base::saturated_cast<uint32_t>(v8_rss_kb_);
  dump->extra_stats[kExtraStatMallocRss] =
      base::saturated_cast<uint32_t>(malloc_rss_kb_);
  dump->extra_stats[kExtraStatCodeOtherRss] =
      base::saturated_cast<uint32_t>(code_other_rss_kb_);
  dump->extra_stats[kExtraStatFontsRss] =
      base::saturated_cast<uint32_t>(fonts_rss_kb_);
  dump->extra_stats[kExtraStatAshmemJitRss] =
      base::saturated_cast<uint32_t>(ashmem_jit_rss_kb_);
  dump->extra_stats[kExtraStatAndroidRuntimeRss] =
      base::saturated_cast<uint32_t>(android_runtime_rss_kb_);
  dump->extra_stats[kExtraStatStacksRss] =
      base::saturated_cast<uint32_t>(stacks_rss_kb_);

  // Reset accumulators for next dump.
  libchrobalt_pss_kb_ = 0;
  libchrobalt_rss_kb_ = 0;
  pa_rss_kb_ = 0;
  v8_rss_kb_ = 0;
  malloc_rss_kb_ = 0;
  code_other_rss_kb_ = 0;
  fonts_rss_kb_ = 0;
  ashmem_jit_rss_kb_ = 0;
  android_runtime_rss_kb_ = 0;
  stacks_rss_kb_ = 0;
#endif
}

}  // namespace cobalt
