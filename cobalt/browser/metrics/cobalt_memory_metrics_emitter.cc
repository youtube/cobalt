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
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"blink_gc",
     "BlinkGC.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"blink_gc",
     "BlinkGC.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     kEffectiveSize,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::CalculationType::kFragmentation,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"blink_gc/main",
     "BlinkGC.Main.Heap.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     kEffectiveSize,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::CalculationType::kFragmentation,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"blink_objects/Document",
     "NumberOfDocuments",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/Frame",
     "NumberOfFrames",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/LayoutObject",
     "NumberOfLayoutObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     MemoryAllocatorDump::kNameObjectCount,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"blink_objects/Node",
     "NumberOfNodes",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     MemoryAllocatorDump::kNameObjectCount,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kCountsInUkmAndSizeInUma,
     {}},
    {"font_caches/shape_caches",
     "FontCaches",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     "size",
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"java_heap",
     "JavaHeap",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"leveldatabase",
     "LevelDatabase",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc",
     "Malloc",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc",
     "Malloc.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc/partitions/allocator/thread_cache",
     "Malloc.ThreadCache",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"malloc/metadata_fragmentation_caches",
     "Malloc.Metadata",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc",
     "PartitionAlloc",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc/allocated_objects",
     "PartitionAlloc.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Partitions.FastMalloc",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc/partitions/fast_malloc",
     "PartitionAlloc.Fragmentation.FastMalloc",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     kEffectiveSize,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::CalculationType::kFragmentation,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.Partitions.Buffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.Partitions.ArrayBuffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"skia",
     "Skia",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"skia/sk_glyph_cache",
     "Skia.SkGlyphCache",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     "size",
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"sqlite",
     "Sqlite",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"ui",
     "UI",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"v8",
     "V8",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kEffectiveSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
    {"v8",
     "V8.AllocatedObjects",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUkmAndUma,
     {}},
#if BUILDFLAG(IS_ANDROID)
    {base::android::MeminfoDumpProvider::kDumpName,
     "AndroidOtherPss",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     base::android::MeminfoDumpProvider::kPssMetricName,
     nullptr,
     CobaltMemoryMetricsEmitter::CalculationType::kValue,
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

  if (memory_dump_in_progress_) {
    return;
  }
  if (!global_dump_) {
    if (callback_for_testing_) {
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
    HistogramProcessType ptype;
    switch (pmd.process_type()) {
      case memory_instrumentation::mojom::ProcessType::BROWSER:
        ptype = HistogramProcessType::kBrowser;
        break;
      case memory_instrumentation::mojom::ProcessType::RENDERER:
        ptype = HistogramProcessType::kRenderer;
        break;
      case memory_instrumentation::mojom::ProcessType::GPU:
        ptype = HistogramProcessType::kGpu;
        break;
      case memory_instrumentation::mojom::ProcessType::UTILITY:
        ptype = HistogramProcessType::kUtility;
        break;
      default:
        ptype = HistogramProcessType::kUtility;
        break;
    }

    private_footprint_total_kb += pmd.os_dump().private_footprint_kb;
    shared_footprint_total_kb += pmd.os_dump().shared_footprint_kb;
    resident_set_total_kb += pmd.os_dump().resident_set_kb;
    private_footprint_swap_total_kb += pmd.os_dump().private_footprint_swap_kb;
    vm_size_total_kb += pmd.os_dump().vm_size_kb;

    for (const auto& item : kAllocatorDumpNamesForMetrics) {
      std::optional<uint64_t> primary_value =
          pmd.GetMetric(item.dump_name, item.metric);
      if (!primary_value) {
        continue;
      }

      uint64_t final_value = 0;
      bool valid = false;

      switch (item.calculation) {
        case CalculationType::kValue:
          final_value = *primary_value;
          valid = true;
          break;
        case CalculationType::kFragmentation: {
          std::optional<uint64_t> secondary_value =
              pmd.GetMetric(item.dump_name, item.secondary_metric);
          if (secondary_value && *primary_value > 0) {
            uint64_t diff = (*primary_value > *secondary_value)
                                ? (*primary_value - *secondary_value)
                                : 0;
            final_value = static_cast<uint64_t>(diff * 100 / (*primary_value));
            final_value = std::min(final_value, static_cast<uint64_t>(100));
            valid = true;
          }
          break;
        }
        case CalculationType::kWasted: {
          std::optional<uint64_t> secondary_value =
              pmd.GetMetric(item.dump_name, item.secondary_metric);
          if (secondary_value) {
            final_value = (*primary_value > *secondary_value)
                              ? (*primary_value - *secondary_value)
                              : 0;
            valid = true;
          }
          break;
        }
      }

      if (valid) {
        EmitProcessUma(ptype, item, final_value);
      }
    }

    const char* process_name = HistogramProcessTypeToString(ptype);
    base::UmaHistogramMemoryLargeMB(
        std::string(kMemoryHistogramPrefix) + process_name + ".ResidentSet",
        static_cast<int>(pmd.os_dump().resident_set_kb / kKiB));
    base::UmaHistogramMemoryLargeMB(
        GetPrivateFootprintHistogramName(ptype),
        static_cast<int>(pmd.os_dump().private_footprint_kb / kKiB));
    base::UmaHistogramMemoryLargeMB(
        std::string(kMemoryHistogramPrefix) + process_name +
            ".SharedMemoryFootprint",
        static_cast<int>(pmd.os_dump().shared_footprint_kb / kKiB));

#if BUILDFLAG(IS_ANDROID)
    if (pmd.os_dump().detailed_stats_kb) {
      std::string prefix =
          base::StrCat({kMemoryHistogramPrefix, process_name, "."});
      std::string exp_prefix = base::StrCat(
          {kExperimentalUmaPrefix, process_name, kVersionSuffixNormal});

      auto get_detailed_stat = [&](const std::string& key) -> uint32_t {
        auto it = pmd.os_dump().detailed_stats_kb->find(key);
        return (it != pmd.os_dump().detailed_stats_kb->end()) ? it->second : 0;
      };

      auto emit_accurate_rss = [&](const char* name, const std::string& key) {
        uint32_t value_kb = get_detailed_stat(key);
        base::UmaHistogramMemoryLargeMB(base::StrCat({prefix, name, "Rss"}),
                                        static_cast<int>(value_kb / kKiB));
        base::UmaHistogramMemoryLargeMB(
            base::StrCat({exp_prefix, name, ".Rss"}),
            static_cast<int>(value_kb / kKiB));
      };

      base::UmaHistogramMemoryLargeMB(
          base::StrCat({prefix, "LibChrobaltPss"}),
          static_cast<int>(get_detailed_stat("pss:cobalt_core") / kKiB));
      base::UmaHistogramMemoryLargeMB(
          base::StrCat({prefix, "LibChrobaltRss"}),
          static_cast<int>(get_detailed_stat("rss:cobalt_core") / kKiB));

      emit_accurate_rss("PartitionAlloc", "rss:partition_alloc");
      emit_accurate_rss("Malloc", "rss:malloc");
      emit_accurate_rss("CodeOther", "rss:code_other");
      emit_accurate_rss("Fonts", "rss:fonts");
      emit_accurate_rss("AshmemJit", "rss:ashmem_jit");
      emit_accurate_rss("AndroidRuntime", "rss:android_runtime");
      emit_accurate_rss("Stacks", "rss:stacks");

      // Emit V8 accurate RSS as supplemental.
      base::UmaHistogramMemoryLargeMB(
          base::StrCat({exp_prefix, "V8.Rss"}),
          static_cast<int>(get_detailed_stat("rss:v8") / kKiB));
    }
#endif
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
  // VM specific metrics
  base::UmaHistogramMemoryLargeMB(
      "Memory.Total.PrivateFootprintSwap",
      static_cast<int>(private_footprint_swap_total_kb / kKiB));
  base::UmaHistogramMemoryLargeMB("Memory.Total.VmSize",
                                  static_cast<int>(vm_size_total_kb / kKiB));

  global_dump_ = nullptr;

  if (callback_for_testing_) {
    std::move(callback_for_testing_).Run();
  }
}

}  // namespace cobalt
