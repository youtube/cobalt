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
#include "partition_alloc/buildflags.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/meminfo_dump_provider.h"
#endif

// Virtual address (VA) space fragmentation telemetry is only actionable on
// 32-bit platforms, where the user-space address range is limited to ~3GB and
// allocators can abort even when physical memory is available.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
#include <inttypes.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
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
    // ==========================================
    // 1. Main Malloc Partition (General Heap under PA-E)
    // ==========================================
    {"malloc/partitions/allocator",
     "Malloc.CommittedSize.Allocator",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "virtual_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.AllocatedObjects.Allocator",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.MaxCommittedSize.Allocator",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "max_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
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
    // ==========================================
    // 2. ArrayBuffer (V8 Typed Arrays / Media Caching)
    // ==========================================
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.CommittedSize.ArrayBuffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "virtual_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"partition_alloc/partitions/array_buffer",
     "PartitionAlloc.AllocatedObjects.ArrayBuffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},

    // ==========================================
    // 3. Buffer (Standard Vectors / Layout / Blink Core)
    // ==========================================
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.CommittedSize.Buffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "virtual_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.AllocatedObjects.Buffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     kAllocatedObjectsSize,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"partition_alloc/partitions/buffer",
     "PartitionAlloc.MaxCommittedSize.Buffer",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "max_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
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

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    {"malloc/partitions/allocator/thread_cache",
     "Malloc.ThreadCache",
     CobaltMemoryMetricsEmitter::MetricSize::kSmall,
     "size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.MaxAllocatedSize",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "max_allocated_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.MaxCommittedSize",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "max_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.CommittedSize",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "virtual_committed_size",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.Wasted",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     "wasted",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc/partitions/allocator",
     "Malloc.Fragmentation",
     CobaltMemoryMetricsEmitter::MetricSize::kPercentage,
     "fragmentation",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {"malloc",
     "Malloc.SyscallsPerMinute",
     CobaltMemoryMetricsEmitter::MetricSize::kTiny,
     "syscalls_per_minute",
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(IS_ANDROID)
    {base::android::MeminfoDumpProvider::kDumpName,
     "AndroidOtherPss",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     base::android::MeminfoDumpProvider::kPssMetricName,
     CobaltMemoryMetricsEmitter::EmitTo::kSizeInUmaOnly,
     {}},
    {base::android::MeminfoDumpProvider::kDumpName,
     "AndroidGraphicsMemory",
     CobaltMemoryMetricsEmitter::MetricSize::kLarge,
     base::android::MeminfoDumpProvider::kGraphicsMetricName,
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

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
// Longest stretch of a /proc/self/maps line the parser looks at in one piece.
// Matches base/profiler/stack_base_address_posix.cc, which reads
// /proc/self/maps on Android with the same 1024-byte budget.
inline constexpr size_t kMaxLineLength = 1024;

// seq_file generates /proc files a page at a time, so a page-sized read is the
// natural granularity.
inline constexpr size_t kReadChunkSize = 4096;

// The kernel appends a "gate VMA" after the process's mappings ([vectors] on
// ARM, [vsyscall] on x86-64). Mirrors ContainsGateVMA() and the early break in
// ReadProcMaps(); see base/debug/proc_maps_linux.cc.
bool IsGateVma(std::string_view line) {
  if (line.ends_with("\n")) {
    line.remove_suffix(1);
  }
  return line.ends_with(" [vectors]") || line.ends_with(" [vsyscall]");
}

// Running totals while walking /proc/self/maps.
struct VmaWalkState {
  uintptr_t prev_vm_end = 0;
  uintptr_t largest_free_gap = 0;
  uint64_t total_unmapped_va = 0;
  size_t vma_count = 0;
  bool first_vma = true;
};

// Folds one maps line into `state`. Returns false at the gate VMA, meaning the
// walk should stop.
bool ConsumeMapsLine(const char* line, VmaWalkState* state) {
  if (IsGateVma(line)) {
    return false;
  }

  uintptr_t vm_start = 0;
  uintptr_t vm_end = 0;
  if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR, &vm_start, &vm_end) != 2) {
    return true;
  }

  // The kernel always emits VMAs in ascending, non-overlapping order, so
  // anything that goes backwards is not a real maps entry.
  if (vm_end < vm_start) {
    return true;
  }
  if (!state->first_vma && vm_start < state->prev_vm_end) {
    return true;
  }

  state->vma_count++;
  if (!state->first_vma && vm_start > state->prev_vm_end) {
    uintptr_t gap = vm_start - state->prev_vm_end;
    if (gap > state->largest_free_gap) {
      state->largest_free_gap = gap;
    }
    state->total_unmapped_va += gap;
  }
  state->first_vma = false;
  state->prev_vm_end = vm_end;
  return true;
}

// `read_chunk` returns the number of bytes written into the buffer, or nullopt
// on error.
std::optional<CobaltMemoryMetricsEmitter::VirtualAddressSpaceMetrics>
CalculateVirtualAddressSpaceMetricsInternal(
    base::FunctionRef<std::optional<size_t>(base::span<uint8_t> /*buffer*/)>
        read_chunk) {
  VmaWalkState state;
  bool reached_gate_vma = false;

  // sscanf() needs a NUL terminator, so a line occupies at most
  // kMaxLineLength-1 bytes and the last slot is reserved.
  char line[kMaxLineLength];
  size_t line_length = 0;
  uint8_t chunk[kReadChunkSize];

  while (!reached_gate_vma) {
    const std::optional<size_t> bytes_read = read_chunk(base::span(chunk));
    if (!bytes_read.has_value() || *bytes_read == 0) {
      break;
    }
    for (uint8_t byte : base::span(chunk).first(*bytes_read)) {
      line[line_length++] = static_cast<char>(byte);
      if (byte != '\n' && line_length != kMaxLineLength - 1) {
        continue;
      }
      line[line_length] = '\0';
      line_length = 0;
      if (!ConsumeMapsLine(line, &state)) {
        reached_gate_vma = true;
        break;
      }
    }
  }

  if (state.vma_count == 0) {
    return std::nullopt;
  }

  CobaltMemoryMetricsEmitter::VirtualAddressSpaceMetrics metrics;
  metrics.vma_count = state.vma_count;
  metrics.largest_free_gap_mb = state.largest_free_gap / kMiB;
  metrics.total_unmapped_va_mb = state.total_unmapped_va / kMiB;

  if (state.total_unmapped_va > 0) {
    double ratio = 1.0 - (static_cast<double>(state.largest_free_gap) /
                          static_cast<double>(state.total_unmapped_va));
    metrics.fragmentation_ratio_pct =
        std::clamp(static_cast<int>(std::round(ratio * 100.0)), 0, 100);
  } else {
    // No unmapped space at all between the lowest and highest mapping.
    metrics.fragmentation_ratio_pct = 100;
  }

  return metrics;
}

void EmitVirtualAddressSpaceMetrics() {
  base::File maps(base::FilePath("/proc/self/maps"),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!maps.IsValid()) {
    DLOG(WARNING) << "Failed to open /proc/self/maps for VA metrics: "
                  << base::File::ErrorToString(maps.error_details());
    return;
  }

  auto metrics = CalculateVirtualAddressSpaceMetricsInternal(
      [&maps](base::span<uint8_t> buffer) {
        return maps.ReadAtCurrentPos(buffer);
      });

  if (!metrics) {
    return;
  }

  // These accumulators are 64-bit and the histogram API takes an int. Clamping
  // rather than wrapping matters because a wrapped value goes negative.
  base::UmaHistogramMemoryLargeMB(
      "Memory.Experimental.VirtualAddress.LargestFreeGapMb",
      base::saturated_cast<int>(metrics->largest_free_gap_mb));

  base::UmaHistogramMemoryLargeMB(
      "Memory.Experimental.VirtualAddress.TotalUnmappedVaMb",
      base::saturated_cast<int>(metrics->total_unmapped_va_mb));

  base::UmaHistogramPercentage(
      "Memory.Experimental.VirtualAddress.FragmentationRatio",
      metrics->fragmentation_ratio_pct);

  base::UmaHistogramCounts100000("Memory.Experimental.VirtualAddress.VmaCount",
                                 base::saturated_cast<int>(metrics->vma_count));
}
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)

}  // namespace

CobaltMemoryMetricsEmitter::CobaltMemoryMetricsEmitter() {
  // The emitter is created on the main thread but will be used
  // on a background sequence maintained by base::SequenceBound
  // in CobaltMetricsServiceClient.
  DETACH_FROM_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_ANDROID)
  base::android::MeminfoDumpProvider::Initialize();
#endif
}

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
#if !BUILDFLAG(IS_IOS_TVOS)
    // TODO: b/497706115 - This field does not exist on tvOS.
    private_footprint_swap_total_kb += pmd.os_dump().private_footprint_swap_kb;
#endif  // !BUILDFLAG(IS_IOS_TVOS)
    vm_size_total_kb += pmd.os_dump().vm_size_kb;

    // Manually calculate fragmentation for individual processes as it may not
    // be present in the dump.
    const uint64_t blink_gc_bytes =
        pmd.GetMetric("blink_gc", kEffectiveSize).value_or(0);
    const uint64_t blink_gc_allocated_objects_bytes =
        pmd.GetMetric("blink_gc", kAllocatedObjectsSize).value_or(0);
    if (blink_gc_bytes > 0) {
      uint64_t fragmentation =
          (blink_gc_bytes > blink_gc_allocated_objects_bytes)
              ? blink_gc_bytes - blink_gc_allocated_objects_bytes
              : 0;
      int fragmentation_pct =
          static_cast<int>(fragmentation * 100 / blink_gc_bytes);
      static const Metric kBlinkGCFragMetric = {
          "blink_gc",      "BlinkGC.Fragmentation", MetricSize::kPercentage,
          "fragmentation", EmitTo::kSizeInUmaOnly,  {}};
      EmitProcessUma(ptype, kBlinkGCFragMetric, fragmentation_pct);
    }

    const uint64_t blink_gc_main_bytes =
        pmd.GetMetric("blink_gc/main", kEffectiveSize).value_or(0);
    const uint64_t blink_gc_main_allocated_objects_bytes =
        pmd.GetMetric("blink_gc/main", kAllocatedObjectsSize).value_or(0);
    if (blink_gc_main_bytes > 0) {
      uint64_t fragmentation =
          (blink_gc_main_bytes > blink_gc_main_allocated_objects_bytes)
              ? blink_gc_main_bytes - blink_gc_main_allocated_objects_bytes
              : 0;
      int fragmentation_pct =
          static_cast<int>(fragmentation * 100 / blink_gc_main_bytes);
      static const Metric kBlinkGCMainFragMetric = {
          "blink_gc/main",         "BlinkGC.Main.Heap.Fragmentation",
          MetricSize::kPercentage, "fragmentation",
          EmitTo::kSizeInUmaOnly,  {}};
      EmitProcessUma(ptype, kBlinkGCMainFragMetric, fragmentation_pct);
    }

    for (const auto& item : kAllocatorDumpNamesForMetrics) {
      // Skip the standard metrics if we are overriding them with
      // the more accurate RSS values below.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
      if (std::string_view(item.uma_name) == "PartitionAlloc" ||
          std::string_view(item.uma_name) == "V8" ||
          std::string_view(item.uma_name) == "Malloc") {
        continue;
      }
#endif
      std::optional<uint64_t> value =
          pmd.GetMetric(item.dump_name, item.metric);
      if (value) {
        EmitProcessUma(ptype, item, value.value());
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
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

    auto get_detailed_stat = [&](const char* name) -> uint32_t {
      if (pmd.os_dump().detailed_stats_kb) {
        auto it = pmd.os_dump().detailed_stats_kb->find(name);
        if (it != pmd.os_dump().detailed_stats_kb->end()) {
          return base::saturated_cast<uint32_t>(it->second);
        }
      }
      return 0;
    };

    uint32_t lib_pss = get_detailed_stat("pss:lib_chrobalt");
    uint32_t lib_rss = get_detailed_stat("rss:lib_chrobalt");

    base::UmaHistogramMemoryLargeMB(base::StrCat({prefix, "LibChrobaltPss"}),
                                    static_cast<int>(lib_pss / kKiB));
    base::UmaHistogramMemoryLargeMB(base::StrCat({prefix, "LibChrobaltRss"}),
                                    static_cast<int>(lib_rss / kKiB));

    emit_accurate_rss("PartitionAlloc",
                      get_detailed_stat("rss:partition_alloc"));
    emit_accurate_rss("Malloc", get_detailed_stat("rss:malloc"));
#if BUILDFLAG(IS_ANDROID)
    emit_accurate_rss("CodeOther", get_detailed_stat("rss:code_other"));
    emit_accurate_rss("Fonts", get_detailed_stat("rss:fonts"));
    emit_accurate_rss("AshmemJit", get_detailed_stat("rss:ashmem_jit"));
    emit_accurate_rss("AndroidRuntime",
                      get_detailed_stat("rss:android_runtime"));
#endif  // BUILDFLAG(IS_ANDROID)
    emit_accurate_rss("Stacks", get_detailed_stat("rss:stacks"));

    // Override V8 with accurate RSS.
    base::UmaHistogramMemoryLargeMB(
        base::StrCat({exp_prefix, "V8"}),
        static_cast<int>(get_detailed_stat("rss:v8") / kKiB));

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
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
  // Walking /proc/self/maps blocks and visits every VMA in the process, so it
  // must not run on this sequence, which is USER_BLOCKING.
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&EmitVirtualAddressSpaceMetrics));
#endif
  // UMA metrics for media buffer memory usage
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

// static
std::optional<CobaltMemoryMetricsEmitter::VirtualAddressSpaceMetrics>
CobaltMemoryMetricsEmitter::CalculateVirtualAddressSpaceMetricsForTesting(
    const std::string& maps_content) {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
  // Hands the canned text over in chunks exactly as base::File would, so the
  // tests drive the same line splitter the production reader uses.
  size_t pos = 0;
  auto read_chunk = [&maps_content, &pos](base::span<uint8_t> buffer) {
    const size_t count = std::min(buffer.size(), maps_content.size() - pos);
    buffer.first(count).copy_from(
        base::as_byte_span(maps_content).subspan(pos, count));
    pos += count;
    return std::optional<size_t>(count);
  };
  return CalculateVirtualAddressSpaceMetricsInternal(read_chunk);
#else
  return std::nullopt;
#endif
}

}  // namespace cobalt
