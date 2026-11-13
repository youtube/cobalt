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

#ifndef COBALT_BROWSER_METRICS_COBALT_STORAGE_METRICS_HELPER_H_
#define COBALT_BROWSER_METRICS_COBALT_STORAGE_METRICS_HELPER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace cobalt {

struct ProcessIoCounters {
  int64_t read_bytes = -1;
  int64_t write_bytes = -1;
};

// Parses Linux /proc/self/io text content to extract block-layer read_bytes
// and write_bytes counters. Returns true if both fields are present and valid.
bool ParseProcIoCounters(std::string_view proc_io_contents,
                         ProcessIoCounters* out_counters);

// Reads /proc/self/io for the current browser process. Returns true if
// block-layer read_bytes and write_bytes were successfully parsed. Returns
// false on platforms without /proc/self/io.
bool ReadCurrentProcessIoCounters(ProcessIoCounters* out_counters);

// The consumers that the cache directory (base::DIR_CACHE) is split into for
// size accounting. Every file in the directory belongs to exactly one bucket.
enum class CacheDirBucket {
  // HTTP cache: <cache_dir>/Cache.
  kHttpCache = 0,
  // V8 JavaScript code cache: <cache_dir>/Code Cache/js.
  kJsCodeCache,
  // WebAssembly code cache: <cache_dir>/Code Cache/wasm.
  kWasmCodeCache,
  // <cache_dir>/Service Worker, which holds CacheStorage.
  kServiceWorker,
  // GPU shader disk caches: <cache_dir>/GrShaderCache and
  // <cache_dir>/ShaderCache.
  kGpuShaderCache,
  // Everything else in the cache directory.
  kOther,
  kMaxValue = kOther,
};

inline constexpr size_t kCacheDirBucketCount =
    static_cast<size_t>(CacheDirBucket::kMaxValue) + 1;

// Returns the name of |bucket| as used in histogram names, e.g. "HttpCache" in
// Cobalt.Storage.DirCache.HttpCache.StartupSizeKB.
std::string_view GetCacheDirBucketName(CacheDirBucket bucket);

// On-disk size of each bucket of the cache directory, in bytes.
struct CacheDirBucketSizes {
  int64_t& operator[](CacheDirBucket bucket) {
    return bytes[static_cast<size_t>(bucket)];
  }
  int64_t operator[](CacheDirBucket bucket) const {
    return bytes[static_cast<size_t>(bucket)];
  }

  // The size of the whole cache directory, which is the sum of all buckets.
  int64_t TotalBytes() const;

  std::array<int64_t, kCacheDirBucketCount> bytes = {};
};

// Walks |cache_dir| once and returns the size of each bucket. The total is the
// same as base::ComputeDirectorySize(cache_dir).
// Must be called on a task runner that allows blocking I/O.
CacheDirBucketSizes ComputeCacheDirBucketSizes(const base::FilePath& cache_dir);

// Emits startup process block I/O histograms first (from |io_counters| if
// valid) and then computes and emits USER_DATA and DIR_CACHE footprint
// histograms, including one per CacheDirBucket. Returns the DIR_CACHE bucket
// sizes that were recorded, or nullopt if |cache_dir| does not exist.
// Must be called on a task runner that allows blocking I/O.
std::optional<CacheDirBucketSizes> RecordStartupStorageAndIoMetricsForPaths(
    const base::FilePath& user_data_dir,
    const base::FilePath& cache_dir,
    const ProcessIoCounters* io_counters);

// Emits Cobalt.Storage.ProcessIo.WriteRateKBPerMin for a post-startup interval
// [baseline_time, current_time]. Returns false without emitting if the
// interval is empty or the write counter went backwards.
bool RecordSteadyStateWriteRateForInterval(int64_t baseline_write_bytes,
                                           base::TimeTicks baseline_time,
                                           int64_t current_write_bytes,
                                           base::TimeTicks current_time);

// Schedules the deferred (+30s) post-startup storage footprint and block I/O
// telemetry task and periodic (every 5 minutes) steady-state write-rate
// sampling on a BEST_EFFORT background sequence.
void ScheduleCobaltStorageMetricsRecording();

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STORAGE_METRICS_HELPER_H_
