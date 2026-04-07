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

#include "cobalt/browser/metrics/cobalt_memory_metrics_emitter.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "media/base/media_client.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/meminfo_dump_provider.h"
#endif

using base::trace_event::MemoryAllocatorDump;
using memory_instrumentation::GetPrivateFootprintHistogramName;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::HistogramProcessType;
using memory_instrumentation::HistogramProcessTypeToString;
using memory_instrumentation::kMemoryHistogramPrefix;

namespace cobalt {

namespace {

const char kEffectiveSize[] = "effective_size";
const char kAllocatedObjectsSize[] = "allocated_objects_size";

constexpr int kKiB = 1024;
constexpr int kMiB = 1024 * 1024;

const CobaltMemoryMetricsEmitter::Metric kAllocatorDumpNamesForMetrics[] = {
    {"blink_gc",
     "BlinkGC",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"blink_gc",
     "BlinkGC.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"blink_gc",
     "BlinkGC.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     "fragmentation",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"blink_gc/main",
     "BlinkGC.Main.Heap.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     "fragmentation",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"blink_objects/Document",
     "NumberOfDocuments",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/Frame",
     "NumberOfFrames",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/LayoutObject",
     "NumberOfLayoutObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/Node",
     "NumberOfNodes",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     MemoryAllocatorDump::kNameObjectCount,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"font_caches/shape_caches",
     "FontCaches",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     "size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"java_heap",
     "JavaHeap",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"leveldatabase",
     "LevelDatabase",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc",
     "Malloc",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc",
     "Malloc.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc",
     "Malloc.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {},
     CobaltMemoryMetricsEmitter::CalculationType::kFragmentation},
    {"malloc",
     "Malloc.Wasted",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {},
     CobaltMemoryMetricsEmitter::CalculationType::kWasted},
    {"partition_alloc",
     "PartitionAlloc",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc/allocated_objects",
     "PartitionAlloc.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"skia",
     "Skia",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"skia/sk_glyph_cache",
     "Skia.SkGlyphCache",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     "size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"sqlite",
     "Sqlite",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"ui",
     "UI",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"v8",
     "V8",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"v8",
     "V8.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"v8",
     "V8.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {},
     CobaltMemoryMetricsEmitter::CalculationType::kFragmentation},
    {"v8",
     "V8.Wasted",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {},
     CobaltMemoryMetricsEmitter::CalculationType::kWasted},
#if BUILDFLAG(IS_ANDROID)
    {base::android::MeminfoDumpProvider::kDumpName,
     "AndroidOtherPss",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     base::android::MeminfoDumpProvider::kPssMetricName,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
#endif
};

constexpr char kExperimentalUmaPrefix[] = "Memory.Experimental.";
constexpr char kVersionSuffixNormal[] = "2.";
constexpr char kVersionSuffixSmall[] = "2.Small.";
constexpr char kVersionSuffixTiny[] = "2.Tiny.";

static const char* MetricSizeToVersionSuffix(
    CobaltMemoryMetricsEmitter::MetricSize size) {
  switch (size) {
    case CobaltMemoryMetricsEmitter::MetricSize::kPercentage:
      return kVersionSuffixNormal;
    case CobaltMemoryMetricsEmitter::MetricSize::kLarge:
      return kVersionSuffixNormal;
    case CobaltMemoryMetricsEmitter::MetricSize::kSmall:
      return kVersionSuffixSmall;
    case CobaltMemoryMetricsEmitter::MetricSize::kTiny:
      return kVersionSuffixTiny;
    case CobaltMemoryMetricsEmitter::MetricSize::kCustom:
      return kVersionSuffixNormal;
  }
}

}  // namespace

CobaltMemoryMetricsEmitter::CobaltMemoryMetricsEmitter() = default;

void CobaltMemoryMetricsEmitter::FetchAndEmitProcessMemoryMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memory_dump_in_progress_ = true;

  auto* instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  if (instrumentation) {
    auto callback =
        base::BindOnce(&CobaltMemoryMetricsEmitter::ReceivedMemoryDump, this);
    std::vector<std::string> mad_list;
    for (const auto& metric : kAllocatorDumpNamesForMetrics) {
      mad_list.push_back(metric.dump_name);
    }
    instrumentation->RequestGlobalDump(mad_list, std::move(callback));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&CobaltMemoryMetricsEmitter::ReceivedMemoryDump, this,
                       false, nullptr));
  }
}

CobaltMemoryMetricsEmitter::~CobaltMemoryMetricsEmitter() = default;

void CobaltMemoryMetricsEmitter::ReceivedMemoryDump(
    bool success,
    std::unique_ptr<GlobalMemoryDump> dump) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memory_dump_in_progress_ = false;
  if (!success || !dump) {
    if (callback_for_testing_) {
      std::move(callback_for_testing_).Run();
    }
    return;
  }
  global_dump_ = std::move(dump);
  CollateResults();
}

// static
void CobaltMemoryMetricsEmitter::EmitProcessUma(
    HistogramProcessType process_type,
    const Metric& item,
    uint64_t value) {
  std::string uma_name = base::StrCat(
      {kExperimentalUmaPrefix, HistogramProcessTypeToString(process_type),
       MetricSizeToVersionSuffix(item.metric_size), item.uma_name});

  switch (item.metric_size) {
    case MetricSize::kPercentage:
      base::UmaHistogramPercentage(uma_name, static_cast<int>(value));
      break;
    case MetricSize::kLarge:
      base::UmaHistogramMemoryLargeMB(uma_name, static_cast<int>(value / kMiB));
      break;
    case MetricSize::kSmall:
      base::UmaHistogramMemoryKB(uma_name, static_cast<int>(value / kKiB));
      break;
    case MetricSize::kTiny:
      base::UmaHistogramCustomCounts(uma_name, static_cast<int>(value), 1,
                                     500000, 100);
      break;
    case MetricSize::kCustom:
      base::UmaHistogramCustomCounts(uma_name, static_cast<int>(value),
                                     item.range.min, item.range.max, 100);
      break;
  }
}

void CobaltMemoryMetricsEmitter::CollateResults() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (memory_dump_in_progress_ || !global_dump_) {
    if (!memory_dump_in_progress_ && callback_for_testing_) {
      std::move(callback_for_testing_).Run();
    }
    return;
  }

  uint64_t private_footprint_total_kb = 0;
  uint64_t shared_footprint_total_kb = 0;
  uint64_t resident_set_total_kb = 0;
  uint64_t private_footprint_swap_total_kb = 0;
  uint64_t vm_size_total_kb = 0;

  for (const auto& pmd : global_dump_->process_dumps()) {
    HistogramProcessType ptype = GetProcessType(pmd.process_type());

    private_footprint_total_kb += pmd.os_dump().private_footprint_kb;
    shared_footprint_total_kb += pmd.os_dump().shared_footprint_kb;
    resident_set_total_kb += pmd.os_dump().resident_set_kb;
    private_footprint_swap_total_kb += pmd.os_dump().private_footprint_swap_kb;
    vm_size_total_kb += pmd.os_dump().vm_size_kb;

    EmitProcessMetrics(ptype, pmd);
  }

  base::UmaHistogramMemoryLargeMB(
      "Memory.Total.ResidentSet",
      static_cast<int>(resident_set_total_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      "Memory.Total.PrivateMemoryFootprint",
      static_cast<int>(private_footprint_total_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      "Memory.Total.SharedMemoryFootprint",
      static_cast<int>(shared_footprint_total_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      "Memory.Total.PrivateFootprintSwap",
      static_cast<int>(private_footprint_swap_total_kb / kKiB));
  base::UmaHistogramMemoryLargeMB("Memory.Total.VmSize",
                                  static_cast<int>(vm_size_total_kb / kKiB));

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  uint64_t encoded_memory_bytes =
      media::MediaClient::GetMediaSourceTotalAllocatedMemory();
  base::UmaHistogramMemoryMB("Memory.Media.AllocatedEncodedBuffer",
                             static_cast<int>(encoded_memory_bytes / kMiB));
#endif

  global_dump_ = nullptr;

  if (callback_for_testing_) {
    std::move(callback_for_testing_).Run();
  }
}

HistogramProcessType CobaltMemoryMetricsEmitter::GetProcessType(
    memory_instrumentation::mojom::ProcessType type) {
  switch (type) {
    case memory_instrumentation::mojom::ProcessType::BROWSER:
      return HistogramProcessType::kBrowser;
    case memory_instrumentation::mojom::ProcessType::RENDERER:
      return HistogramProcessType::kRenderer;
    case memory_instrumentation::mojom::ProcessType::GPU:
      return HistogramProcessType::kGpu;
    case memory_instrumentation::mojom::ProcessType::UTILITY:
      return HistogramProcessType::kUtility;
    default:
      return HistogramProcessType::kUtility;
  }
}

void CobaltMemoryMetricsEmitter::EmitProcessMetrics(
    HistogramProcessType ptype,
    const memory_instrumentation::GlobalMemoryDump::ProcessDump& pmd) {
  const char* process_name = HistogramProcessTypeToString(ptype);

  // 1. Core OS metrics.
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({kMemoryHistogramPrefix, process_name, ".ResidentSet"}),
      static_cast<int>(pmd.os_dump().resident_set_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      GetPrivateFootprintHistogramName(ptype),
      static_cast<int>(pmd.os_dump().private_footprint_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      base::StrCat(
          {kMemoryHistogramPrefix, process_name, ".SharedMemoryFootprint"}),
      static_cast<int>(pmd.os_dump().shared_footprint_kb / kKiB));

  // 2. Fragmentation metrics (manually calculated).
  auto emit_frag = [&](const char* dump_name, const char* uma_name) {
    const uint64_t bytes = pmd.GetMetric(dump_name, kEffectiveSize).value_or(0);
    const uint64_t allocated_bytes =
        pmd.GetMetric(dump_name, kAllocatedObjectsSize).value_or(0);
    if (bytes > 0) {
      uint64_t fragmentation =
          (bytes > allocated_bytes) ? bytes - allocated_bytes : 0;
      int fragmentation_pct = static_cast<int>(fragmentation * 100 / bytes);
      static const Metric kFragMetric = {
          "", uma_name, MetricSize::kPercentage, "", EmitTo::kSizeInUmaOnly,
          {}};
      EmitProcessUma(ptype, kFragMetric, fragmentation_pct);
    }
  };
  emit_frag("blink_gc", "BlinkGC.Fragmentation");
  emit_frag("blink_gc/main", "BlinkGC.Main.Heap.Fragmentation");

  // 3. Allocator metrics from GlobalMemoryDump.
  bool has_accurate_rss = false;
#if BUILDFLAG(IS_ANDROID)
  has_accurate_rss = true;
#endif

  for (const auto& item : kAllocatorDumpNamesForMetrics) {
    // Skip if we have more accurate RSS-based metrics (Android only).
    if (has_accurate_rss &&
        (std::string_view(item.uma_name) == "PartitionAlloc" ||
         std::string_view(item.uma_name) == "V8" ||
         std::string_view(item.uma_name) == "Malloc")) {
      continue;
    }

    absl::optional<uint64_t> value = pmd.GetMetric(item.dump_name, item.metric);
    if (value) {
      uint64_t final_value = *value;
      if (item.calculation_type == CalculationType::kFragmentation) {
        uint64_t total =
            pmd.GetMetric(item.dump_name, kEffectiveSize).value_or(0);
        final_value = (total > *value) ? (total - *value) * 100 / total : 0;
      } else if (item.calculation_type == CalculationType::kWasted) {
        uint64_t total =
            pmd.GetMetric(item.dump_name, kEffectiveSize).value_or(0);
        final_value = (total > *value) ? (total - *value) : 0;
      }
      EmitProcessUma(ptype, item, final_value);
    }
  }

  // 4. Detailed OS metrics (accurate RSS/PSS breakdown).
#if BUILDFLAG(IS_ANDROID)
  std::string prefix =
      base::StrCat({kMemoryHistogramPrefix, process_name, "."});
  std::string exp_prefix = base::StrCat(
      {kExperimentalUmaPrefix, process_name, kVersionSuffixNormal});

  auto emit_accurate_rss = [&](const char* name, uint32_t value_kb) {
    base::UmaHistogramMemoryLargeMB(base::StrCat({prefix, name, "Rss"}),
                                    static_cast<int>(value_kb / kKiB));
    base::UmaHistogramMemoryLargeMB(base::StrCat({exp_prefix, name}),
                                    static_cast<int>(value_kb / kKiB));
  };

  base::UmaHistogramMemoryLargeMB(
      base::StrCat({prefix, "LibChrobaltPss"}),
      static_cast<int>(pmd.os_dump().libchrobalt_pss_kb / kKiB));
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({prefix, "LibChrobaltRss"}),
      static_cast<int>(pmd.os_dump().libchrobalt_rss_kb / kKiB));

  auto get_alloc_kb = [&](const char* dump_name) -> uint32_t {
    return static_cast<uint32_t>(
        pmd.GetMetric(dump_name, kEffectiveSize).value_or(0) / kKiB);
  };

  if (pmd.os_dump().detailed_stats) {
    const auto& categories_kb = pmd.os_dump().detailed_stats->categories_kb;
    auto emit_detailed =
        [&](const char* name,
            memory_instrumentation::mojom::CobaltMemoryCategory category,
            uint32_t fallback_val, uint32_t alloc_fallback_kb = 0) {
          auto it = categories_kb.find(category);
          uint32_t val =
              (it != categories_kb.end()) ? it->second : fallback_val;
          if (val == 0 && alloc_fallback_kb > 0) {
            val = alloc_fallback_kb;
          }
          emit_accurate_rss(name, val);
        };

    emit_detailed(
        "PartitionAlloc",
        memory_instrumentation::mojom::CobaltMemoryCategory::kPartitionAlloc,
        pmd.os_dump().partition_alloc_rss_kb, get_alloc_kb("partition_alloc"));
    emit_detailed("Malloc",
                  memory_instrumentation::mojom::CobaltMemoryCategory::kMalloc,
                  pmd.os_dump().malloc_rss_kb, get_alloc_kb("malloc"));
    emit_detailed(
        "CodeOther",
        memory_instrumentation::mojom::CobaltMemoryCategory::kCodeOther,
        pmd.os_dump().code_other_rss_kb);
    emit_detailed("Fonts",
                  memory_instrumentation::mojom::CobaltMemoryCategory::kFonts,
                  pmd.os_dump().fonts_rss_kb);
    emit_detailed(
        "AshmemJit",
        memory_instrumentation::mojom::CobaltMemoryCategory::kAshmemJit,
        pmd.os_dump().ashmem_jit_rss_kb);
    emit_detailed(
        "AndroidRuntime",
        memory_instrumentation::mojom::CobaltMemoryCategory::kAndroidRuntime,
        pmd.os_dump().android_runtime_rss_kb);
    emit_detailed("Stacks",
                  memory_instrumentation::mojom::CobaltMemoryCategory::kStacks,
                  pmd.os_dump().stacks_rss_kb);
    emit_detailed("V8",
                  memory_instrumentation::mojom::CobaltMemoryCategory::kV8,
                  pmd.os_dump().v8_rss_kb, get_alloc_kb("v8"));
  } else {
    auto emit_with_alloc_fallback = [&](const char* name, uint32_t smaps_val,
                                        const char* dump_name) {
      uint32_t val = smaps_val;
      if (val == 0) {
        val = get_alloc_kb(dump_name);
      }
      emit_accurate_rss(name, val);
    };

    emit_with_alloc_fallback("PartitionAlloc",
                             pmd.os_dump().partition_alloc_rss_kb,
                             "partition_alloc");
    emit_with_alloc_fallback("Malloc", pmd.os_dump().malloc_rss_kb, "malloc");
    emit_accurate_rss("CodeOther", pmd.os_dump().code_other_rss_kb);
    emit_accurate_rss("Fonts", pmd.os_dump().fonts_rss_kb);
    emit_accurate_rss("AshmemJit", pmd.os_dump().ashmem_jit_rss_kb);
    emit_accurate_rss("AndroidRuntime", pmd.os_dump().android_runtime_rss_kb);
    emit_accurate_rss("Stacks", pmd.os_dump().stacks_rss_kb);

    // Override V8 with accurate RSS, or fallback to allocator dump.
    uint32_t v8_val = pmd.os_dump().v8_rss_kb;
    if (v8_val == 0) {
      v8_val = get_alloc_kb("v8");
    }
    base::UmaHistogramMemoryLargeMB(base::StrCat({exp_prefix, "V8"}),
                                    static_cast<int>(v8_val / kKiB));
  }
#endif
}

}  // namespace cobalt
