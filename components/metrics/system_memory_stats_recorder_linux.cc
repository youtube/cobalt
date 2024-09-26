// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_memory_stats_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

namespace metrics {

// Record a size in megabytes, a potential interval from 250MB up to 32 GB.
#define UMA_HISTOGRAM_ALLOCATED_MEGABYTES(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 250, 32768, 50)

// Records a statistics |sample| for UMA histogram |name|
// using a linear distribution of buckets.
#define UMA_HISTOGRAM_LINEAR(name, sample, max, buckets)                       \
  STATIC_HISTOGRAM_POINTER_BLOCK(                                              \
      name, Add(sample),                                                       \
      base::LinearHistogram::FactoryGet(                                       \
          name,                                                                \
          1, /* Minimum. The 0 bin for underflow is automatically added. */    \
          max + 1,     /* Ensure bucket size of |maximum| / |bucket_count|. */ \
          buckets + 2, /*  Account for the underflow and overflow bins. */     \
          base::Histogram::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_MEGABYTES_LINEAR(name, sample) \
  UMA_HISTOGRAM_LINEAR(name, sample, 2500, 50)

void RecordMemoryStats(RecordMemoryStatsType type) {
#if BUILDFLAG(IS_CHROMEOS)
  // Record graphics GEM object size in a histogram with 50 MB buckets.
  int mem_gpu_mb = 0;
  bool mem_gpu_result = false;
  base::GraphicsMemoryInfoKB gpu_memory;
  mem_gpu_result = base::GetGraphicsMemoryInfo(&gpu_memory);

  if (mem_gpu_result) {
    mem_gpu_mb = gpu_memory.gpu_memory_size / 1024 / 1024;
    switch (type) {
      case RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED:
        UMA_HISTOGRAM_MEGABYTES_LINEAR("Memory.OOMKill.Contents.MemGraphicsMB",
                                       mem_gpu_mb);
        break;
      case RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED:
        UMA_HISTOGRAM_MEGABYTES_LINEAR(
            "Memory.OOMKill.Extensions.MemGraphicsMB", mem_gpu_mb);
        break;
    }
  }
#endif

  base::SystemMemoryInfoKB memory;
  if (base::GetSystemMemoryInfo(&memory)) {
    // On Intel, graphics objects are in anonymous pages, but on ARM they are
    // not. For a total "allocated count" add in graphics pages on ARM.
    int mem_allocated_mb = (memory.active_anon + memory.inactive_anon) / 1024;
#if BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
    mem_allocated_mb += mem_gpu_mb;
#endif

    int mem_available_mb =
        (memory.active_file + memory.inactive_file + memory.free) / 1024;

    switch (type) {
      case RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED: {
        UMA_HISTOGRAM_ALLOCATED_MEGABYTES(
            "Memory.OOMKill.Contents.MemAllocatedMB", mem_allocated_mb);
        UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.OOMKill.Contents.MemAvailableMB",
                                      mem_available_mb);
        break;
      }
      case RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED: {
        UMA_HISTOGRAM_ALLOCATED_MEGABYTES(
            "Memory.OOMKill.Extensions.MemAllocatedMB", mem_allocated_mb);
        UMA_HISTOGRAM_LARGE_MEMORY_MB(
            "Memory.OOMKill.Extensions.MemAvailableMB", mem_available_mb);
        break;
      }
    }

#if BUILDFLAG(IS_CHROMEOS)
    // Record shared memory (used by renderer/GPU buffers).
    int mem_shmem_mb = memory.shmem / 1024;
    switch (type) {
      case RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED:
        UMA_HISTOGRAM_MEGABYTES_LINEAR("Memory.OOMKill.Contents.MemShmemMB",
                                       mem_shmem_mb);
        break;
      case RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED:
        UMA_HISTOGRAM_MEGABYTES_LINEAR("Memory.OOMKill.Extensions.MemShmemMB",
                                       mem_shmem_mb);
        break;
    }
#endif
  }
}

}  // namespace metrics
