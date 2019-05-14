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

#include "cobalt/browser/memory_tracker/tool/compressed_time_series_tool.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"
#include "starboard/common/log.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

///////////////////////////////////////////////////////////////////////////////
CompressedTimeSeriesTool::CompressedTimeSeriesTool() : number_samples_(400) {}

void CompressedTimeSeriesTool::Run(Params* params) {
  TimeSeries timeseries;
  Timer timer_status_message(base::TimeDelta::FromSeconds(1));

  // Outputs CSV once every minute.
  Timer timer_output_csv(base::TimeDelta::FromMinutes(1));

  // Initial sample time is every 50 milliseconds.
  base::TimeDelta current_sample_interval =
      base::TimeDelta::FromMilliseconds(50);

  while (!params->wait_for_finish_signal(current_sample_interval.ToSbTime())) {
    AcquireSample(params->memory_tracker(), &timeseries,
                  params->time_since_start());

    if (IsFull(timeseries, number_samples_)) {
      Compress(&timeseries);           // Remove every other element.
      current_sample_interval *= 2;    // Double the sample time.
      timer_status_message.Restart();  // Skip status message.
    }

    if (timer_output_csv.UpdateAndIsExpired()) {
      const std::string str = ToCsvString(timeseries);
      params->logger()->Output(str.c_str());
      timer_status_message.Restart();  // Skip status message.
    }

    // Print status message.
    if (timer_status_message.UpdateAndIsExpired()) {
      std::stringstream ss;
      ss << tool_name() << " is running..." << kNewLine;
      params->logger()->Output(ss.str().c_str());
    }
  }
}

std::string CompressedTimeSeriesTool::ToCsvString(
    const TimeSeries& timeseries) {
  size_t largest_sample_size = 0;
  size_t smallest_sample_size = INT_MAX;

  typedef MapAllocationSamples::const_iterator MapIt;

  // Sanitize samples_in and store as samples.
  const MapAllocationSamples& samples_in = timeseries.samples_;
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

  // Preamble
  ss << kNewLine << "//////////////////////////////////////////////" << kNewLine
     << "// CSV of BYTES allocated per region (MB's)." << kNewLine
     << "// Units are in Megabytes. This is designed" << kNewLine
     << "// to be used in a stacked graph." << kNewLine;

  // HEADER.
  ss << kQuote << "Time(mins)" << kQuote << kDelimiter;
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    const std::string& name = it->first;
    ss << kQuote << SanitizeCSVKey(name) << kQuote << kDelimiter;
  }
  ss << kNewLine;

  // Print out the values of each of the samples.
  for (size_t i = 0; i < smallest_sample_size; ++i) {
    // Output time first so that it can be used as an x-axis.
    const double time_mins =
        timeseries.time_stamps_[i].InMilliseconds() / (1000. * 60.);
    ss << time_mins << ",";
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      const int64 alloc_bytes = it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = static_cast<double>(alloc_bytes / (1000 * 10));
      n = n / (100.);
      ss << n << kDelimiter;
    }
    ss << kNewLine;
  }
  ss << "// END CSV of BYTES allocated per region." << kNewLine;
  ss << "//////////////////////////////////////////////";

  ss << kNewLine;
  // Preamble
  ss << kNewLine << "//////////////////////////////////////////////";
  ss << kNewLine << "// CSV of COUNT of allocations per region." << kNewLine;

  // HEADER
  ss << kQuote << "Time(mins)" << kQuote << kDelimiter;
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    const std::string& name = it->first;
    ss << kQuote << SanitizeCSVKey(name) << kQuote << kDelimiter;
  }
  ss << kNewLine;
  for (size_t i = 0; i < smallest_sample_size; ++i) {
    // Output time first so that it can be used as an x-axis.
    const double time_mins =
        timeseries.time_stamps_[i].InMilliseconds() / (1000. * 60.);
    ss << time_mins << ",";
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      const int64 n_allocs = it->second.number_allocations_[i];
      ss << n_allocs << kDelimiter;
    }
    ss << kNewLine;
  }
  ss << "// END CSV of COUNT of allocations per region." << kNewLine;
  ss << "//////////////////////////////////////////////";
  ss << kNewLine << kNewLine;

  std::string output = ss.str();
  return output;
}

void CompressedTimeSeriesTool::AcquireSample(
    nb::analytics::MemoryTracker* memory_tracker, TimeSeries* timeseries,
    const base::TimeDelta& time_now) {
  const size_t sample_count = timeseries->time_stamps_.size();
  timeseries->time_stamps_.push_back(time_now);

  MapAllocationSamples& map_samples = timeseries->samples_;

  std::vector<const nb::analytics::AllocationGroup*> vector_output;
  memory_tracker->GetAllocationGroups(&vector_output);

  // Sample all known memory scopes.
  for (size_t i = 0; i < vector_output.size(); ++i) {
    const nb::analytics::AllocationGroup* group = vector_output[i];
    const std::string& name = group->name();

    const bool first_creation =
        map_samples.find(group->name()) == map_samples.end();

    AllocationSamples& new_entry = map_samples[name];

    // Didn't see it before so create new entry.
    if (first_creation) {
      // Make up for lost samples...
      new_entry.allocated_bytes_.resize(sample_count, 0);
      new_entry.number_allocations_.resize(sample_count, 0);
    }

    int32 num_allocs = -1;
    int64 allocation_bytes = -1;
    group->GetAggregateStats(&num_allocs, &allocation_bytes);

    new_entry.allocated_bytes_.push_back(allocation_bytes);
    new_entry.number_allocations_.push_back(num_allocs);
  }

  // Sample the in use memory bytes reported by malloc.
  nb::analytics::MemoryStats memory_stats =
      nb::analytics::GetProcessMemoryStats();
  AllocationSamples& process_mem_in_use = map_samples["ProcessMemoryInUse"];
  process_mem_in_use.allocated_bytes_.push_back(memory_stats.used_cpu_memory);
  process_mem_in_use.number_allocations_.push_back(0);

  // Sample the reserved memory bytes reported by malloc.
  AllocationSamples& process_mem_reserved =
      map_samples["ProcessMemoryReserved"];
  process_mem_reserved.allocated_bytes_.push_back(memory_stats.used_cpu_memory);
  process_mem_reserved.number_allocations_.push_back(0);
}

bool CompressedTimeSeriesTool::IsFull(const TimeSeries& timeseries,
                                      size_t samples_limit) {
  return timeseries.time_stamps_.size() >= samples_limit;
}

void CompressedTimeSeriesTool::Compress(TimeSeries* timeseries) {
  typedef MapAllocationSamples::iterator MapIt;
  MapAllocationSamples& samples = timeseries->samples_;
  RemoveOddElements(&(timeseries->time_stamps_));
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    AllocationSamples& data = it->second;
    RemoveOddElements(&data.allocated_bytes_);
    RemoveOddElements(&data.number_allocations_);
  }
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
