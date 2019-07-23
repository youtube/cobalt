// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/print_csv_tool.h"

#include <sstream>
#include <string>
#include <vector>

#include "cobalt/base/c_val.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "starboard/common/log.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

PrintCSVTool::PrintCSVTool(int sampling_interval_ms, int sampling_time_ms)
    : sample_interval_ms_(sampling_interval_ms),
      sampling_time_ms_(sampling_time_ms) {}

std::string PrintCSVTool::ToCsvString(const MapAllocationSamples& samples_in) {
  typedef MapAllocationSamples Map;
  typedef Map::const_iterator MapIt;

  size_t largest_sample_size = 0;
  size_t smallest_sample_size = INT_MAX;

  // Sanitize samples_in and store as samples.
  MapAllocationSamples samples;
  for (MapIt it = samples_in.begin(); it != samples_in.end(); ++it) {
    std::string name = it->first;
    const AllocationSamples& value = it->second;

    if (value.allocated_bytes_.size() != value.number_allocations_.size()) {
      SB_NOTREACHED() << "Error at " << __FILE__ << ":" << __LINE__;
      return "ERROR";
    }

    const size_t n = value.allocated_bytes_.size();
    if (n > largest_sample_size) {
      largest_sample_size = n;
    }
    if (n < smallest_sample_size) {
      smallest_sample_size = n;
    }

    const bool duplicate_found = (samples.end() != samples.find(name));
    if (duplicate_found) {
      SB_NOTREACHED() << "Error, duplicate found for entry: " << name
                      << kNewLine;
    }
    // Store value as a sanitized sample.
    samples[name] = value;
  }

  SB_DCHECK(largest_sample_size == smallest_sample_size);

  std::stringstream ss;

  // Begin output to CSV.
  // Sometimes we need to skip the CPU memory entry.
  const MapIt total_cpu_memory_it = samples.find(UntrackedMemoryKey());

  // Preamble
  ss << kNewLine << "//////////////////////////////////////////////";
  ss << kNewLine << "// CSV of bytes / allocation" << kNewLine;
  // HEADER.
  ss << "Name" << kDelimiter << kQuote << "Bytes/Alloc" << kQuote << kNewLine;
  // DATA.
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }

    const AllocationSamples& samples_ref = it->second;
    if (samples_ref.allocated_bytes_.empty() ||
        samples_ref.number_allocations_.empty()) {
      SB_NOTREACHED() << "Should not be here";
      return "ERROR";
    }
    const int64 n_allocs = samples_ref.number_allocations_.back();
    const int64 n_bytes = samples_ref.allocated_bytes_.back();
    int64 bytes_per_alloc = 0;
    if (n_allocs > 0) {
      bytes_per_alloc = n_bytes / n_allocs;
    }
    const std::string& name = it->first;
    ss << kQuote << SanitizeCSVKey(name) << kQuote << kDelimiter
       << bytes_per_alloc << kNewLine;
  }
  ss << kNewLine;

  // Preamble
  ss << kNewLine << "//////////////////////////////////////////////" << kNewLine
     << "// CSV of bytes allocated per region (MB's)." << kNewLine
     << "// Units are in Megabytes. This is designed" << kNewLine
     << "// to be used in a stacked graph." << kNewLine;

  // HEADER.
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }
    // Strip out any characters that could make parsing the csv difficult.
    const std::string name = SanitizeCSVKey(it->first);
    ss << kQuote << name << kQuote << kDelimiter;
  }
  // Save the total for last.
  if (total_cpu_memory_it != samples.end()) {
    const std::string& name = SanitizeCSVKey(total_cpu_memory_it->first);
    ss << kQuote << name << kQuote << kDelimiter;
  }
  ss << kNewLine;

  // Print out the values of each of the samples.
  for (size_t i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      if (total_cpu_memory_it == it) {
        continue;
      }
      const int64 alloc_bytes = it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = static_cast<double>(alloc_bytes / (1000 * 10));
      n = n / (100.);
      ss << n << kDelimiter;
    }
    if (total_cpu_memory_it != samples.end()) {
      const int64 alloc_bytes = total_cpu_memory_it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = static_cast<double>(alloc_bytes / (1000 * 10));
      n = n / (100.);
      ss << n << kDelimiter;
    }
    ss << kNewLine;
  }

  ss << kNewLine;
  // Preamble
  ss << kNewLine << "//////////////////////////////////////////////";
  ss << kNewLine << "// CSV of number of allocations per region." << kNewLine;

  // HEADER
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }
    const std::string& name = SanitizeCSVKey(it->first);
    ss << kQuote << name << kQuote << kDelimiter;
  }
  ss << kNewLine;
  for (size_t i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      if (total_cpu_memory_it == it) {
        continue;
      }
      const int64 n_allocs = it->second.number_allocations_[i];
      ss << n_allocs << kDelimiter;
    }
    ss << kNewLine;
  }
  std::string output = ss.str();
  return output;
}

const char* PrintCSVTool::UntrackedMemoryKey() { return "Untracked Memory"; }

void PrintCSVTool::Run(Params* params) {
  params->logger()->Output("\nMemoryTrackerPrintCSVThread is sampling...\n");
  int sample_count = 0;
  MapAllocationSamples map_samples;

  while (!TimeExpiredYet(*params) && !params->finished()) {
    // Sample total memory used by the system.
    nb::analytics::MemoryStats mem_stats =
        nb::analytics::GetProcessMemoryStats();
    int64 untracked_used_memory =
        mem_stats.used_cpu_memory + mem_stats.used_gpu_memory;

    std::vector<const nb::analytics::AllocationGroup*> vector_output;
    params->memory_tracker()->GetAllocationGroups(&vector_output);

    // Sample all known memory scopes.
    for (size_t i = 0; i < vector_output.size(); ++i) {
      const nb::analytics::AllocationGroup* group = vector_output[i];
      const std::string& name = group->name();

      const bool first_creation =
          map_samples.find(group->name()) == map_samples.end();

      AllocationSamples* new_entry = &(map_samples[name]);

      // Didn't see it before so create new entry.
      if (first_creation) {
        // Make up for lost samples...
        new_entry->allocated_bytes_.resize(sample_count, 0);
        new_entry->number_allocations_.resize(sample_count, 0);
      }

      int32 num_allocs = -1;
      int64 allocation_bytes = -1;
      group->GetAggregateStats(&num_allocs, &allocation_bytes);

      new_entry->allocated_bytes_.push_back(allocation_bytes);
      new_entry->number_allocations_.push_back(num_allocs);

      untracked_used_memory -= allocation_bytes;
    }

    // Now push in remaining total.
    AllocationSamples* process_sample = &(map_samples[UntrackedMemoryKey()]);
    if (untracked_used_memory < 0) {
      // On some platforms, total GPU memory may not be correctly reported.
      // However the allocations from the GPU memory may be reported. In this
      // case untracked_used_memory will go negative. To protect the memory
      // reporting the untracked_used_memory is set to 0 so that it doesn't
      // cause an error in reporting.
      untracked_used_memory = 0;
    }
    process_sample->allocated_bytes_.push_back(untracked_used_memory);
    process_sample->number_allocations_.push_back(-1);

    ++sample_count;
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(sample_interval_ms_));
  }

  std::stringstream ss;
  ss.precision(2);
  ss << "Time now: " << params->TimeInMinutesString() << ",\n";
  ss << ToCsvString(map_samples);
  params->logger()->Output(ss.str().c_str());
  params->logger()->Flush();
  // Prevents the "thread exited code 0" from being interleaved into the
  // output. This happens if flush is not implemented correctly in the system.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
}

bool PrintCSVTool::TimeExpiredYet(const Params& params) {
  base::TimeDelta dt = params.time_since_start();
  int64 dt_ms = dt.InMilliseconds();
  const bool expired_time = dt_ms > sampling_time_ms_;
  return expired_time;
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
