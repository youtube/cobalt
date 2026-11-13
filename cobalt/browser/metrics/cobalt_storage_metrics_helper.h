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

#include <cstdint>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace cobalt {

struct ProcessIoCounters {
  int64_t read_bytes = -1;
  int64_t write_bytes = -1;
  // Page faults that had to read from storage. -1 if unavailable (only
  // getrusage() reports it).
  int64_t major_page_faults = -1;
};

// Parses Linux /proc/self/io text content to extract block-layer read_bytes
// and write_bytes counters. Returns true if both fields are present and valid.
bool ParseProcIoCounters(std::string_view proc_io_contents,
                         ProcessIoCounters* out_counters);

// Converts getrusage() ru_inblock, ru_oublock (512-byte blocks) and ru_majflt
// to ProcessIoCounters. Returns false if the values are negative, or if both
// block counts are 0, which means the kernel does not account task I/O (a
// browser process always writes some data in its first seconds).
bool ProcessIoCountersFromRusage(int64_t in_blocks,
                                 int64_t out_blocks,
                                 int64_t major_page_faults,
                                 ProcessIoCounters* out_counters);

// Reads the block I/O counters of the current browser process. Returns false
// if they are unavailable.
// - Android: getrusage(RUSAGE_SELF). It reports the same kernel counters as
//   /proc/self/io read_bytes and write_bytes, but unlike /proc/self/io it is
//   readable when the process is not dumpable, as on release
//   (non-debuggable) APKs.
// - Other Starboard platforms: /proc/self/io.
bool ReadCurrentProcessIoCounters(ProcessIoCounters* out_counters);

// Enables base's I/O jank monitoring (base::EnableIOJankMonitoringForProcess)
// for the browser process and records
// Cobalt.Responsiveness.IOJankyIntervalsPerMinute and
// Cobalt.Responsiveness.IOJanksTotalPerMinute once per minute. Call at most
// once per process.
void EnableCobaltIOJankMonitoring();

// Emits startup process block I/O histograms first (from |io_counters| if
// valid) and then computes and emits USER_DATA and DIR_CACHE footprint
// histograms.
// Must be called on a task runner that allows blocking I/O.
void RecordStartupStorageAndIoMetricsForPaths(
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

// Schedules the deferred (+10s) post-startup storage footprint and block I/O
// telemetry task and periodic (every 5 minutes) steady-state write-rate
// sampling on a BEST_EFFORT background sequence.
void ScheduleCobaltStorageMetricsRecording();

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STORAGE_METRICS_HELPER_H_
