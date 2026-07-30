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
};

// Parses Linux /proc/self/io text content to extract block-layer read_bytes
// and write_bytes counters. Returns true if both fields are present and valid.
bool ParseProcIoCounters(std::string_view proc_io_contents,
                         ProcessIoCounters* out_counters);

// Reads /proc/self/io for the current browser process. Returns true if
// block-layer read_bytes and write_bytes were successfully parsed. Returns
// false on platforms without /proc/self/io.
bool ReadCurrentProcessIoCounters(ProcessIoCounters* out_counters);

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

// Schedules the deferred (+30s) post-startup storage footprint and block I/O
// telemetry task and periodic (every 5 minutes) steady-state write-rate
// sampling on a BEST_EFFORT background sequence.
void ScheduleCobaltStorageMetricsRecording();

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STORAGE_METRICS_HELPER_H_
