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

#include "cobalt/browser/memory_tracker/tool/leak_finder_tool.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <utility>

#include "base/timer/timer.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "cobalt/script/util/stack_trace_helpers.h"
#include "nb/memory_scope.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Number of output values to display in the csv.
const size_t kNumberOfOutputColumns = 40;

LeakFinderTool::LeakFinderTool(StackTraceMode mode)
    : string_pool_(128),
      frame_map_(128),
      callframe_map_(128),
      stack_trace_mode_(mode) {
  default_callframe_str_ = &string_pool_.Intern("<Unknown>");
}

LeakFinderTool::~LeakFinderTool() {
  frame_map_.Clear();
  callframe_map_.Clear();
}

bool LeakFinderTool::IsJavascriptScope(
    const nb::analytics::CallStack& callstack) {
  // March through all MemoryScopes in the callstack and check if any of them
  // contains a javascript scope. If it does return true.
  for (nb::analytics::CallStack::const_iterator it = callstack.begin();
       it != callstack.end(); ++it) {
    const NbMemoryScopeInfo* memory_scope = *it;
    const bool is_javascript_scope =
        strstr(memory_scope->memory_scope_name_, "Javascript");
    if (is_javascript_scope) {
      return true;
    }
  }
  return false;
}

void LeakFinderTool::OnMemoryAllocation(
    const void* memory_block, const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack) {
  // When in javascript mode, filter only allocations with "Javascript" in
  // the memory scope name.
  if (stack_trace_mode_ == kJavascript) {
    if (!IsJavascriptScope(callstack)) {
      return;
    }
  }

  // symbol_str can be used as a unique key. The same value of callstack will
  // always produce the same string pointer.
  const std::string* symbol_str = GetOrCreateSymbol(callstack);
  // Track the allocation.
  if (!callframe_map_.SetIfMissing(memory_block, symbol_str)) {
    CallFrameMap::EntryHandle entry_handle;
    callframe_map_.Get(memory_block, &entry_handle);

    const void* ptr = entry_handle.Valid() ? entry_handle.Key() : NULL;
    entry_handle.ReleaseLockAndInvalidate();

    DCHECK(false) << "Unexpected memory block at " << memory_block
                  << " already existed as: " << ptr;
  }

  AllocationFrameMap::EntryHandle entry_handle;
  frame_map_.GetOrCreate(symbol_str, &entry_handle);

  // While this entry_handle is in use, no other threads
  // can modify this entry.
  AllocRec& alloc_rec = entry_handle.ValueMutable();
  alloc_rec.total_bytes += record.size;
  alloc_rec.num_allocs++;
}

void LeakFinderTool::OnMemoryDeallocation(
    const void* memory_block, const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack) {

  const std::string* symbol_str = NULL;

  {
    CallFrameMap::EntryHandle entry_handle;
    if (!callframe_map_.Get(memory_block, &entry_handle)) {
      // This happens if the allocation happened before this tool attached
      // to the memory tracker or if the memory allocation was filtered and
      // therefore isn't being tracked.
      return;
    }
    symbol_str = entry_handle.Value();
    callframe_map_.Remove(&entry_handle);
  }

  // This case can happen if the memory tracker attaches after the allocation.
  if (!symbol_str) {
    return;
  }

  AllocationFrameMap::EntryHandle entry_handle;
  frame_map_.GetOrCreate(symbol_str, &entry_handle);

  // While entry_handle is in use, no other element can modify
  // this element.
  AllocRec& alloc_rec = entry_handle.ValueMutable();
  alloc_rec.total_bytes -= record.size;
  alloc_rec.num_allocs--;
}

std::string LeakFinderTool::tool_name() const {
  return "MemoryTrackerLeakFinder";
}

void LeakFinderTool::Run(Params* params) {
  // Run function does almost nothing.
  params->logger()->Output("MemoryTrackerLeakFinder running...");

  static const size_t kMaxSamples = 400;

  // This value will decay whenever the buffer fills up and is compressed via
  // sample elimination.
  SbTime sample_time = 50;  // 50 microseconds.

  std::vector<base::TimeDelta> time_values;
  std::map<const std::string*, std::vector<AllocRec> > map_allocations;

  SbTime start_time = SbTimeGetMonotonicNow();
  Timer output_trigger(base::TimeDelta::FromMinutes(1));

  const double recording_delay_mins = 5.0;

  // Controls how often an update status message is sent to output.
  Timer output_status_timer(base::TimeDelta::FromSeconds(1));

  while (true) {
    if (params->wait_for_finish_signal(sample_time)) {
      break;  // Finish received, break.
    }
    SbTime now_time = SbTimeGetMonotonicNow();

    const base::TimeDelta time_since_start =
        base::Time::FromSbTime(now_time) - base::Time::FromSbTime(start_time);

    // DELAY RECORDING AS STARTUP MEMORY IS INITIALIZED
    // Cleaner graphs if we wait until after startup.
    //
    const double times_since_start_mins = time_since_start.InSecondsF() / 60.0;
    if (times_since_start_mins < recording_delay_mins) {
      if (output_status_timer.UpdateAndIsExpired()) {
        double remaining_time_mins =
            (recording_delay_mins - times_since_start_mins);
        std::stringstream ss;
        ss << "MemoryTrackerLeakFinder starting in " << remaining_time_mins
           << " minutes.\n";
        params->logger()->Output(ss.str().c_str());
      }
      continue;
    }

    time_values.push_back(time_since_start);

    // To improve performance, make a copy of the values to a vector on
    // the stack.
    std::vector<std::pair<const std::string*, AllocRec> > samples;
    SampleSnapshot(&samples);

    // Take a snapshot.
    for (size_t i = 0; i < samples.size(); ++i) {
      std::pair<const std::string*, AllocRec> sample = samples[i];
      std::vector<AllocRec>& sample_history = map_allocations[sample.first];

      sample_history.resize(time_values.size());
      sample_history.back() = sample.second;
    }

    if (output_trigger.UpdateAndIsExpired() && time_values.size() > 10) {
      // Timer expired so dump the current information output.

      std::vector<AllocationProfile> alloc_profiles;
      GenerateTopLeakingAllocationProfiles(time_values, map_allocations,
                                           &alloc_profiles);
      if (alloc_profiles.empty()) {
        params->logger()->Output(
            "MemoryTrackerLeakFinder: alloc_profiles was "
            "empty and nothing is written.");
      } else {
        if (alloc_profiles.size() > kNumberOfOutputColumns) {
          alloc_profiles.resize(kNumberOfOutputColumns);
        }

        std::string csv_str = GenerateCSV(time_values, alloc_profiles);
        params->logger()->Output(csv_str.c_str());
      }
    }

    // Compress the buffer, and modify sample_time so that it's twice as slow.
    if (time_values.size() >= kMaxSamples) {
      sample_time *= 2;  // Double the time that it takes to trigger a sample.
      // Remove every other element in time_values.
      RemoveOddElements(&time_values);

      // Remove every other element in the samples.
      for (size_t i = 0; i < samples.size(); ++i) {
        std::pair<const std::string*, AllocRec> sample = samples[i];
        std::vector<AllocRec>& sample_history = map_allocations[sample.first];
        RemoveOddElements(&sample_history);
      }
    }
  }
}

const std::string* LeakFinderTool::GetOrCreateSymbol(
    const nb::analytics::CallStack& callstack) {
  const std::string* symbol_str = NULL;

  // In javascript mode we try and get the javascript symbol. Otherwise
  // fallback to C++ symbol.
  if (stack_trace_mode_ == kJavascript) {
    symbol_str = TryGetJavascriptSymbol();
    if (symbol_str) {
      return symbol_str;
    }
  }

  symbol_str = GetOrCreateCplusPlusSymbol(callstack);
  return symbol_str;
}

const std::string* LeakFinderTool::GetOrCreateCplusPlusSymbol(
    const nb::analytics::CallStack& callstack) {
  if (callstack.empty()) {
    return default_callframe_str_;
  } else {
    const NbMemoryScopeInfo* memory_scope = callstack.back();

    const bool skip =
        strstr(memory_scope->function_name_, "js_malloc") ||
        strstr(memory_scope->function_name_, "js_realloc") ||
        strstr(memory_scope->function_name_, "new_");

    // Skip up one callstack because we don't want to track calls to
    // allocation functions.
    if (skip && callstack.size() > 1) {
      memory_scope = callstack[callstack.size() - 2];
    }

    const char* file_name = BaseNameFast(memory_scope->file_name_);

    // Generates a symbol.
    // Example:
    //   "Javascript:Interpreter.cpp(415):RunScript()"
    char symbol_buff[128];
    SbStringFormatF(symbol_buff, sizeof(symbol_buff), "%s:%s(%d)::%s()",
                    memory_scope->memory_scope_name_, file_name,
                    memory_scope->line_number_, memory_scope->function_name_);

    // Get's a unique pointer to a string containing the symbol. If the symbol
    // was previously generated then the previous symbol is returned.
    return &string_pool_.Intern(symbol_buff);
  }
}

const std::string* LeakFinderTool::TryGetJavascriptSymbol() {
  auto* js_stack_gen = script::util::GetThreadLocalStackTraceGenerator();
  if (!js_stack_gen || !js_stack_gen->Valid()) {
    return NULL;
  }

  // Only get one symbol.
  char buffer[256];
  if (!js_stack_gen->GenerateStackTraceString(1, buffer, sizeof(buffer))) {
    return NULL;
  }
  const char* file_name = BaseNameFast(buffer);
  return &string_pool_.Intern(file_name);
}

void LeakFinderTool::SampleSnapshot(
    std::vector<std::pair<const std::string*, AllocRec> >* destination) {
  destination->erase(destination->begin(), destination->end());

  const size_t sample_size = frame_map_.GetSize();

  // Do this locally.
  destination->reserve(sample_size + 10);
  frame_map_.CopyToStdVector(destination);
}

std::string LeakFinderTool::GenerateCSV(
    const std::vector<base::TimeDelta>& time_values,
    const std::vector<AllocationProfile>& data) {
  std::stringstream ss;
  ss << std::fixed;  // Turn off scientific notation for CSV values.
  ss << std::setprecision(3);
  ss << kNewLine << kNewLine;

  // HEADER
  ss << "// Allocation in megabytes. Keep in mind that only" << kNewLine
     << "// the N top allocations will be displayed, sorted" << kNewLine
     << "// by slope steepness. Negative slopping allocations" << kNewLine
     << "// and allocations that have few blocks may be filtered" << kNewLine
     << "// out." << kNewLine << kQuote << "Time(min)" << kQuote << kDelimiter;
  for (size_t i = 0; i < data.size(); ++i) {
    const AllocationProfile& alloc_profile = data[i];
    const std::string& name = *alloc_profile.name_;
    ss << kQuote << name << kQuote << kDelimiter;
  }
  ss << kNewLine;

  // BODY
  for (size_t i = 0; i < time_values.size(); ++i) {
    for (size_t j = 0; j < data.size(); ++j) {
      if (j == 0) {
        double mins = time_values[i].InSecondsF() / 60.f;
        if (mins < .001) {
          mins = 0;
        }
        ss << mins << kDelimiter;
      }

      const AllocationProfile& alloc_profile = data[j];
      const std::vector<AllocRec>& alloc_history =
          *alloc_profile.alloc_history_;

      double megabytes = static_cast<double>(alloc_history[i].total_bytes) /
                         static_cast<double>(1024 * 1024);

      DCHECK_EQ(alloc_history.size(), time_values.size());
      ss << megabytes << kDelimiter;
    }
    ss << kNewLine;
  }
  ss << kNewLine << kNewLine;
  ss << "// Object counts." << kNewLine;
  ss << kQuote << "Time(min)" << kQuote << kDelimiter;
  for (size_t i = 0; i < data.size(); ++i) {
    const AllocationProfile& alloc_profile = data[i];
    const std::string& name = *alloc_profile.name_;
    ss << kQuote << name << kQuote << kDelimiter;
  }
  ss << kNewLine;
  for (size_t i = 0; i < time_values.size(); ++i) {
    for (size_t j = 0; j < data.size(); ++j) {
      if (j == 0) {
        double mins = time_values[i].InSecondsF() / 60.f;
        if (mins < .001) {
          mins = 0;
        }
        ss << mins << kDelimiter;
      }
      const AllocationProfile& alloc_profile = data[j];
      const std::vector<AllocRec>& alloc_history =
          *alloc_profile.alloc_history_;
      DCHECK_EQ(alloc_history.size(), time_values.size());
      ss << alloc_history[i].num_allocs << kDelimiter;
    }
    ss << kNewLine;
  }

  ss << kNewLine << kNewLine;
  return ss.str();
}

void LeakFinderTool::GenerateTopLeakingAllocationProfiles(
    const std::vector<base::TimeDelta>& time_values, const MapSamples& samples,
    std::vector<AllocationProfile>* destination) {
  // GENERATE LINEAR REGRESSION LINE
  // first value is time in microseconds.
  // second value is total_bytes.
  std::vector<std::pair<int64_t, int64_t> > sample_data;

  typedef std::map<const std::string*, SlopeYIntercept> LinearRegressionMap;
  LinearRegressionMap linear_regression_map;

  std::vector<AllocationProfile> allocation_profiles;

  for (MapSamples::const_iterator it = samples.begin(); it != samples.end();
       ++it) {
    const std::string* allocation_name = it->first;
    const std::vector<AllocRec>& allocation_samples = it->second;

    if (allocation_samples.empty()) {
      continue;
    }

    // Filter out allocations records that have low number of allocations.
    if (allocation_samples.back().num_allocs < 10) {
      continue;
    }

    // Filter out allocations that are insignificant.
    int64_t largest_allocation_sample = 0;
    const int64_t kMinAllocationSize = 1024 * 64;
    for (size_t i = 0; i < allocation_samples.size(); ++i) {
      int64_t bytes = allocation_samples[i].total_bytes;
      largest_allocation_sample = std::max(largest_allocation_sample, bytes);
    }
    if (largest_allocation_sample < kMinAllocationSize) {
      continue;
    }

    // Filter out allocations where there is no growth between first quartile
    // and final output.
    const AllocRec& first_quartile_sample =
        allocation_samples[allocation_samples.size() / 4];
    const AllocRec& final_sample = allocation_samples.back();

    const double increase_ratio =
        static_cast<double>(final_sample.total_bytes) /
        static_cast<double>(first_quartile_sample.total_bytes);

    // 5% threshold of increase to be qualified as a lead.
    static const double kMinIncreaseThreshold = .05;

    // If the increase between first quartile and final sample less than 5%
    // then skip.
    if (increase_ratio < kMinIncreaseThreshold) {
      continue;
    }

    sample_data.clear();  // Recycle.
    for (size_t i = 0; i < time_values.size(); ++i) {
      std::pair<int64_t, int64_t> datum(time_values[i].InMicroseconds(),
                                        allocation_samples[i].total_bytes);
      sample_data.push_back(datum);
    }

    double slope = 0;
    double y_intercept = 0;
    bool valid = GetLinearFit(sample_data.begin(), sample_data.end(), &slope,
                              &y_intercept);
    DCHECK(valid);
    linear_regression_map[allocation_name] =
        SlopeYIntercept(slope, y_intercept);
    AllocationProfile alloc_profile(allocation_name, &allocation_samples, slope,
                                    y_intercept);
    alloc_profile.leak_potential_ =
        allocation_samples.back().total_bytes * slope;
    allocation_profiles.push_back(alloc_profile);
  }

  std::sort(allocation_profiles.begin(), allocation_profiles.end(),
            AllocationProfile::CompareLeakPotential);
  // Biggest one first.
  std::reverse(allocation_profiles.begin(), allocation_profiles.end());
  *destination = allocation_profiles;
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
