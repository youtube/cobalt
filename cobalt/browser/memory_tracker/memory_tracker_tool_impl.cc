// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/memory_tracker_tool_impl.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/time.h"
#include "cobalt/base/c_val_collection_entry_stats.h"
#include "cobalt/browser/memory_tracker/buffered_file_writer.h"
#include "cobalt/script/mozjs/util/stack_trace_helpers.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/concurrent_map.h"
#include "nb/memory_scope.h"
#include "starboard/common/semaphore.h"
#include "starboard/configuration.h"
#include "starboard/file.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

using nb::analytics::AllocationGroup;
using nb::analytics::AllocationRecord;
using nb::analytics::AllocationVisitor;
using nb::analytics::GetProcessMemoryStats;
using nb::analytics::MemoryStats;
using nb::analytics::MemoryTracker;

namespace {
const char kQuote[] = "\"";
const char kDelimiter[] = ",";
const char kNewLine[] = "\n";

// This is a simple algorithm to remove the "needle" from the haystack. Note
// that this function is simple and not well optimized.
std::string RemoveString(const std::string& haystack, const char* needle) {
  const size_t kNotFound = std::string::npos;

  // Base case. No modification needed.
  size_t pos = haystack.find(needle);
  if (pos == kNotFound) {
    return haystack;
  }
  const size_t n = strlen(needle);
  std::string output;
  output.reserve(haystack.size());

  // Copy string, omitting the portion containing the "needle".
  std::copy(haystack.begin(), haystack.begin() + pos,
            std::back_inserter(output));
  std::copy(haystack.begin() + pos + n, haystack.end(),
            std::back_inserter(output));

  // Recursively remove same needle in haystack.
  return RemoveString(output, needle);
}

// Not optimized but works ok for a tool that dumps out in user time.
std::string SanitizeCSVKey(std::string key) {
  key = RemoveString(key, kQuote);
  key = RemoveString(key, kDelimiter);
  key = RemoveString(key, kNewLine);
  return key;
}

// Converts "2345.54" => "2,345.54".
std::string InsertCommasIntoNumberString(const std::string& input) {
  typedef std::vector<char> CharVector;
  typedef CharVector::iterator CharIt;

  CharVector chars(input.begin(), input.end());
  std::reverse(chars.begin(), chars.end());

  CharIt curr_it = chars.begin();
  CharIt mid = std::find(chars.begin(), chars.end(), '.');
  if (mid == chars.end()) {
    mid = curr_it;
  }

  CharVector out(curr_it, mid);

  int counter = 0;
  for (CharIt it = mid; it != chars.end(); ++it) {
    if (counter != 0 && (counter % 3 == 0)) {
      out.push_back(',');
    }
    if (*it != '.') {
      counter++;
    }
    out.push_back(*it);
  }

  std::reverse(out.begin(), out.end());
  std::stringstream ss;
  for (size_t i = 0; i < out.size(); ++i) {
    ss << out[i];
  }
  return ss.str();
}

template <typename T>
std::string NumberFormatWithCommas(T val) {
  // Convert value to string.
  std::stringstream ss;
  ss << val;
  std::string s = InsertCommasIntoNumberString(ss.str());
  return s;
}

// Removes odd elements and resizes vector.
template <typename VectorType>
void RemoveOddElements(VectorType* v) {
  typedef typename VectorType::iterator iterator;

  iterator read_it = v->end();
  iterator write_it = v->end();
  for (size_t i = 0; i*2 < v->size(); ++i) {
    write_it = v->begin() + i;
    read_it = v->begin() + (i*2);
    *write_it = *read_it;
  }
  if (write_it != v->end()) {
    write_it++;
  }
  v->erase(write_it, v->end());
}

// NoMemoryTracking will disable memory tracking while in the current scope of
// execution. When the object is destroyed it will reset the previous state
// of allocation tracking.
// Example:
//   void Foo() {
//     NoMemoryTracking no_memory_tracking_in_scope;
//     int* ptr = new int();  // ptr is not tracked.
//     delete ptr;
//     return;    // Previous memory tracking state is restored.
//   }
class NoMemoryTracking {
 public:
  explicit NoMemoryTracking(nb::analytics::MemoryTracker* owner);
  ~NoMemoryTracking();

 private:
  bool prev_val_;
  nb::analytics::MemoryTracker* owner_;
};

// Simple timer class that fires once after dt time has elapsed.
class Timer {
 public:
  explicit Timer(base::TimeDelta dt)
      : start_time_(base::TimeTicks::Now()), time_before_expiration_(dt) {}

  void Restart() { start_time_ = base::TimeTicks::Now(); }

  bool UpdateAndIsExpired() {
    base::TimeTicks now_time = base::TimeTicks::Now();
    base::TimeDelta dt = now_time - start_time_;
    if (dt > time_before_expiration_) {
      start_time_ = now_time;
      return true;
    } else {
      return false;
    }
  }

 private:
  base::TimeTicks start_time_;
  base::TimeDelta time_before_expiration_;
};

// Returns true if linear fit was calculated. False otherwise. Reasons for
// returning false include passing in an empty range, such that
// begin_it == end_it, or all x-values being the same.
//
// Algorithm adapted from:
// https://en.wikipedia.org/wiki/Simple_linear_regression#Fitting_the_regression_line
// Example:
//    std::vector<std::pair<int, int> > data;
//    for (int i = 0; i < 10; ++i) {
//      data.push_back(std::pair<int, int>(i+1, 2*i));
//    }
//    double slope = 0;
//    double y_intercept = 0;
//    GetLinearFit(data.begin(), data.end(), &slope, &y_intercept);
//    std::cout << "slope: " << slope << "\n";
//    std::cout << "y_intercept: " << y_intercept<< "\n";
template <typename PairIterator>
bool GetLinearFit(PairIterator begin_it, PairIterator end_it, double* out_slope,
                  double* out_yintercept) {
  if (begin_it == end_it) {
    return false;
  }

  size_t n = 0;
  double x_avg = 0;
  double y_avg = 0;

  for (PairIterator it = begin_it; it != end_it; ++it) {
    x_avg += it->first;
    y_avg += it->second;
    n++;
  }

  x_avg /= n;
  y_avg /= n;

  double numerator = 0;
  double denominator = 0;

  for (PairIterator it = begin_it; it != end_it; ++it) {
    double x_variance = it->first - x_avg;
    double y_variance = it->second - y_avg;
    numerator += (x_variance * y_variance);
    denominator += (x_variance * x_variance);
  }

  if (denominator == 0.0) {
    return false;
  }

  double slope = numerator / denominator;
  double yintercept = y_avg - slope * x_avg;

  *out_slope = slope;
  *out_yintercept = yintercept;
  return true;
}

// Returns a substring with the directory path removed from the filename.
// Example:
//   F::BaseNameFast("directory/filename.cc") => "filename.cc"
//   F::BaseNameFast("directory\filename.cc") => "filename.cc"
//
// Note that base::FilePath::BaseName() isn't used because of performance
// reasons.
const char* BaseNameFast(const char* file_name) {
  const char* end_pos = file_name + strlen(file_name);
  const char* last_forward_slash = SbStringFindLastCharacter(file_name, '/');
  if (last_forward_slash) {
    if (end_pos != last_forward_slash) {
      ++last_forward_slash;
    }
    return last_forward_slash;
  }

  const char* last_backward_slash = SbStringFindLastCharacter(file_name, '\\');
  if (last_backward_slash) {
    if (end_pos != last_backward_slash) {
      ++last_backward_slash;
    }
    return last_backward_slash;
  }
  return file_name;
}

}  // namespace

class Params {
 public:
  Params(nb::analytics::MemoryTracker* memory_tracker, AbstractLogger* logger,
         base::Time start_time)
      : memory_tracker_(memory_tracker),
        finished_(false),
        logger_(logger),
        timer_(start_time) {}
  bool finished() const { return finished_; }
  bool wait_for_finish_signal(SbTime wait_us) {
    return finished_semaphore_.TakeWait(wait_us);
  }
  void set_finished(bool val) {
    finished_ = val;
    finished_semaphore_.Put();
  }

  nb::analytics::MemoryTracker* memory_tracker() const {
    return memory_tracker_;
  }
  AbstractLogger* logger() { return logger_.get(); }
  base::TimeDelta time_since_start() const {
    return base::Time::NowFromSystemTime() - timer_;
  }
  std::string TimeInMinutesString() const {
    base::TimeDelta delta_t = time_since_start();
    int64 seconds = delta_t.InSeconds();
    float time_mins = static_cast<float>(seconds) / 60.f;
    std::stringstream ss;

    ss << time_mins;
    return ss.str();
  }

 private:
  nb::analytics::MemoryTracker* memory_tracker_;
  bool finished_;
  scoped_ptr<AbstractLogger> logger_;
  base::Time timer_;
  starboard::Semaphore finished_semaphore_;
};

MemoryTrackerToolThread::MemoryTrackerToolThread(
    nb::analytics::MemoryTracker* memory_tracker,
    AbstractMemoryTrackerTool* tool, AbstractLogger* logger)
    : Super(tool->tool_name()),
      params_(
          new Params(memory_tracker, logger, base::Time::NowFromSystemTime())),
      tool_(tool) {
  Start();
}

MemoryTrackerToolThread::~MemoryTrackerToolThread() {
  Join();
  tool_.reset();
  params_.reset();
}

void MemoryTrackerToolThread::Join() {
  params_->set_finished(true);
  Super::Join();
}

void MemoryTrackerToolThread::Run() {
  NoMemoryTracking no_mem_tracking_in_this_scope(params_->memory_tracker());
  // This tool will run until the finished_ if flipped to false.
  tool_->Run(params_.get());
}

NoMemoryTracking::NoMemoryTracking(nb::analytics::MemoryTracker* owner)
    :  prev_val_(false), owner_(owner) {
  if (owner_) {
    prev_val_ = owner_->IsMemoryTrackingEnabled();
    owner_->SetMemoryTrackingEnabled(false);
  }
}

NoMemoryTracking::~NoMemoryTracking() {
  if (owner_) {
    owner_->SetMemoryTrackingEnabled(prev_val_);
  }
}

class MemoryTrackerPrint::CvalsMap {
 public:
  typedef base::CVal<base::cval::SizeInBytes> CValType;

  ~CvalsMap() {
    while (!map_.empty()) {
      MapCvals::iterator it = map_.begin();
      delete it->second;
      map_.erase(it);
    }
  }

  void Update(const std::string& name, size_t value) {
    std::string cval_name = GetCvalName(name);
    CValType*& val = map_[cval_name];
    if (!val) {
      // Create if not found.
      val = new CValType(cval_name, 0, "Automatically generated by the "
                                       "browser::memory_tracker system.");
    }
    (*val) = value;
  }

  static std::string GetCvalName(const std::string& name) {
    std::stringstream ss;
    ss << "Memory.Scope." << name;
    return ss.str();
  }

 private:
  typedef std::map<std::string, CValType*> MapCvals;
  MapCvals map_;
};

MemoryTrackerPrint::MemoryTrackerPrint() : cvals_map_(new CvalsMap) {}

MemoryTrackerPrint::~MemoryTrackerPrint() {}

void MemoryTrackerPrint::Run(Params* params) {
  const std::string kSeperator
      = "--------------------------------------------------";

  while (!params->finished()) {
    std::vector<const AllocationGroup*> vector_output;
    params->memory_tracker()->GetAllocationGroups(&vector_output);

    typedef std::map<std::string, const AllocationGroup*> Map;
    typedef Map::const_iterator MapIt;

    Map output;
    for (size_t i = 0; i < vector_output.size(); ++i) {
      const AllocationGroup* group = vector_output[i];
      output[group->name()] = group;
    }

    int32 num_allocs = 0;
    int64 total_bytes = 0;

    struct F {
      static void PrintRow(std::stringstream* ss, const std::string& v1,
                           const std::string& v2, const std::string& v3) {
        ss->width(25);
        *ss << std::left << v1;
        ss->width(13);
        *ss << std::right << v2 << "  ";
        ss->width(10);
        *ss << std::right << v3 << "\n";
      }
    };

    if (params->memory_tracker()->IsMemoryTrackingEnabled()) {
      // If this isn't true then it would cause an infinite loop. The
      // following will likely crash.
      SB_DCHECK(false) << "Unexpected, memory tracking should be disabled.";
    }

    std::stringstream ss;

    ss << kNewLine;
    ss << "TimeNow " << params->TimeInMinutesString()
       << " (minutes):" << kNewLine << kNewLine;

    ss << kSeperator << kNewLine;
    MemoryStats memstats = GetProcessMemoryStats();

    F::PrintRow(&ss, "MALLOC STAT", "IN USE BYTES", "");
    ss << kSeperator << kNewLine;
    F::PrintRow(&ss,
                "Total CPU Reserved",
                NumberFormatWithCommas(memstats.total_cpu_memory),
                "");

    F::PrintRow(&ss,
                "Total CPU Used",
                NumberFormatWithCommas(memstats.used_cpu_memory),
                "");

    F::PrintRow(&ss,
                "Total GPU Reserved",
                NumberFormatWithCommas(memstats.total_gpu_memory),
                "");

    F::PrintRow(&ss,
                "Total GPU Used",
                NumberFormatWithCommas(memstats.used_gpu_memory),
                "");

    ss << kSeperator << kNewLine << kNewLine;

    ss << kSeperator << kNewLine;
    F::PrintRow(&ss, "MEMORY REGION", "IN USE BYTES", "NUM ALLOCS");
    ss << kSeperator << kNewLine;

    for (MapIt it = output.begin(); it != output.end(); ++it) {
      const AllocationGroup* group = it->second;
      if (!group) {
        continue;
      }

      int32 num_group_allocs = -1;
      int64 total_group_bytes = -1;

      group->GetAggregateStats(&num_group_allocs, &total_group_bytes);
      SB_DCHECK(-1 != num_group_allocs);
      SB_DCHECK(-1 != total_group_bytes);

      cvals_map_->Update(group->name(), total_group_bytes);

      num_allocs += num_group_allocs;
      total_bytes += total_group_bytes;

      F::PrintRow(&ss, it->first, NumberFormatWithCommas(total_group_bytes),
                  NumberFormatWithCommas(num_group_allocs));
    }

    cvals_map_->Update("Total", total_bytes);

    ss << kNewLine;

    F::PrintRow(&ss,
                "Total (in groups above)",
                NumberFormatWithCommas(total_bytes),
                NumberFormatWithCommas(num_allocs));

    ss << kSeperator << kNewLine;
    ss << kNewLine << kNewLine;

    params->logger()->Output(ss.str().c_str());
    // Output once every 5 seconds.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));
  }
}

MemoryTrackerPrintCSV::MemoryTrackerPrintCSV(int sampling_interval_ms,
                                             int sampling_time_ms)
    : sample_interval_ms_(sampling_interval_ms),
      sampling_time_ms_(sampling_time_ms) {}

std::string MemoryTrackerPrintCSV::ToCsvString(
    const MapAllocationSamples& samples_in) {
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

    const AllocationSamples& samples = it->second;
    if (samples.allocated_bytes_.empty() ||
        samples.number_allocations_.empty()) {
      SB_NOTREACHED() << "Should not be here";
      return "ERROR";
    }
    const int64 n_allocs = samples.number_allocations_.back();
    const int64 n_bytes = samples.allocated_bytes_.back();
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
      double n = alloc_bytes / (1000 * 10);
      n = n / (100.);
      ss << n << kDelimiter;
    }
    if (total_cpu_memory_it != samples.end()) {
      const int64 alloc_bytes = total_cpu_memory_it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = alloc_bytes / (1000 * 10);
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

const char* MemoryTrackerPrintCSV::UntrackedMemoryKey() {
  return "Untracked Memory";
}

void MemoryTrackerPrintCSV::Run(Params* params) {
  params->logger()->Output("\nMemoryTrackerPrintCSVThread is sampling...\n");
  int sample_count = 0;
  MapAllocationSamples map_samples;

  while (!TimeExpiredYet(*params) && !params->finished()) {
    // Sample total memory used by the system.
    MemoryStats mem_stats = GetProcessMemoryStats();
    int64 untracked_used_memory =
        mem_stats.used_cpu_memory + mem_stats.used_gpu_memory;

    std::vector<const AllocationGroup*> vector_output;
    params->memory_tracker()->GetAllocationGroups(&vector_output);

    // Sample all known memory scopes.
    for (size_t i = 0; i < vector_output.size(); ++i) {
      const AllocationGroup* group = vector_output[i];
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

bool MemoryTrackerPrintCSV::TimeExpiredYet(const Params& params) {
  base::TimeDelta dt = params.time_since_start();
  int64 dt_ms = dt.InMilliseconds();
  const bool expired_time = dt_ms > sampling_time_ms_;
  return expired_time;
}

///////////////////////////////////////////////////////////////////////////////
MemoryTrackerCompressedTimeSeries::MemoryTrackerCompressedTimeSeries()
    : number_samples_(400) {}

void MemoryTrackerCompressedTimeSeries::Run(Params* params) {
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

std::string MemoryTrackerCompressedTimeSeries::ToCsvString(
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
  ss << kNewLine
     << "//////////////////////////////////////////////" << kNewLine
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
      double n = alloc_bytes / (1000 * 10);
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

void MemoryTrackerCompressedTimeSeries::AcquireSample(
    MemoryTracker* memory_tracker, TimeSeries* timeseries,
    const base::TimeDelta& time_now) {
  const size_t sample_count = timeseries->time_stamps_.size();
  timeseries->time_stamps_.push_back(time_now);

  MapAllocationSamples& map_samples = timeseries->samples_;

  std::vector<const AllocationGroup*> vector_output;
  memory_tracker->GetAllocationGroups(&vector_output);

  // Sample all known memory scopes.
  for (size_t i = 0; i < vector_output.size(); ++i) {
    const AllocationGroup* group = vector_output[i];
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
  MemoryStats memory_stats = GetProcessMemoryStats();
  AllocationSamples& process_mem_in_use = map_samples["ProcessMemoryInUse"];
  process_mem_in_use.allocated_bytes_.push_back(memory_stats.used_cpu_memory);
  process_mem_in_use.number_allocations_.push_back(0);

  // Sample the reserved memory bytes reported by malloc.
  AllocationSamples& process_mem_reserved
      = map_samples["ProcessMemoryReserved"];
  process_mem_reserved.allocated_bytes_
      .push_back(memory_stats.used_cpu_memory);
  process_mem_reserved.number_allocations_.push_back(0);
}

bool MemoryTrackerCompressedTimeSeries::IsFull(const TimeSeries& timeseries,
                                               size_t samples_limit) {
  return timeseries.time_stamps_.size() >= samples_limit;
}

void MemoryTrackerCompressedTimeSeries::Compress(TimeSeries* timeseries) {
  typedef MapAllocationSamples::iterator MapIt;
  MapAllocationSamples& samples = timeseries->samples_;
  RemoveOddElements(&(timeseries->time_stamps_));
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    AllocationSamples& data = it->second;
    RemoveOddElements(&data.allocated_bytes_);
    RemoveOddElements(&data.number_allocations_);
  }
}

MemorySizeBinner::MemorySizeBinner(const std::string& memory_scope_name)
    : memory_scope_name_(memory_scope_name) {}

const AllocationGroup* FindAllocationGroup(const std::string& name,
                                           MemoryTracker* memory_tracker) {
  std::vector<const AllocationGroup*> groups;
  memory_tracker->GetAllocationGroups(&groups);
  // Find by exact string match.
  for (size_t i = 0; i < groups.size(); ++i) {
    const AllocationGroup* group = groups[i];
    if (group->name().compare(name) == 0) {
      return group;
    }
  }
  return NULL;
}

void MemorySizeBinner::Run(Params* params) {
  const AllocationGroup* target_group = NULL;

  while (!params->finished()) {
    if (target_group == NULL && !memory_scope_name_.empty()) {
      target_group =
          FindAllocationGroup(memory_scope_name_, params->memory_tracker());
    }

    std::stringstream ss;
    ss.precision(2);
    if (target_group || memory_scope_name_.empty()) {
      AllocationSizeBinner visitor_binner = AllocationSizeBinner(target_group);
      params->memory_tracker()->Accept(&visitor_binner);

      size_t min_size = 0;
      size_t max_size = 0;

      visitor_binner.GetLargestSizeRange(&min_size, &max_size);

      FindTopSizes top_size_visitor =
          FindTopSizes(min_size, max_size, target_group);
      params->memory_tracker()->Accept(&top_size_visitor);

      ss << kNewLine;
      ss << "TimeNow " << params->TimeInMinutesString() << " (minutes):";
      ss << kNewLine;
      if (!memory_scope_name_.empty()) {
        ss << "Tracking Memory Scope \"" << memory_scope_name_ << "\", ";
      } else {
        ss << "Tracking whole program, ";
      }
      ss << "first row is allocation size range, second row is number of "
         << kNewLine << "allocations in that range." << kNewLine;
      ss << visitor_binner.ToCSVString();
      ss << kNewLine;
      ss << "Largest allocation range: \"" << min_size << "..." << max_size
         << "\"" << kNewLine;
      ss << "Printing out top allocations from this range: " << kNewLine;
      ss << top_size_visitor.ToString(5) << kNewLine;
    } else {
      ss << "No allocations for \"" << memory_scope_name_ << "\".";
    }

    params->logger()->Output(ss.str().c_str());
    params->logger()->Flush();

    // Sleep until the next sample.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  }
}

size_t AllocationSizeBinner::GetBucketIndexForAllocationSize(size_t size) {
  for (int i = 0; i < 32; ++i) {
    size_t val = 0x1 << i;
    if (val > size) {
      return i;
    }
  }
  SB_NOTREACHED();
  return 32;
}

void AllocationSizeBinner::GetSizeRange(size_t size, size_t* min_value,
                                        size_t* max_value) {
  size_t idx = GetBucketIndexForAllocationSize(size);
  IndexToSizeRange(idx, min_value, max_value);
}

void AllocationSizeBinner::IndexToSizeRange(size_t idx, size_t* min_value,
                                            size_t* max_value) {
  if (idx == 0) {
    *min_value = 0;
    *max_value = 0;
    return;
  }
  *min_value = 0x1 << (idx - 1);
  *max_value = (*min_value << 1) - 1;
  return;
}

size_t AllocationSizeBinner::GetIndexRepresentingMostMemoryConsumption() const {
  int64 largest_allocation_total = 0;
  size_t largest_allocation_total_idx = 0;

  for (size_t i = 0; i < allocation_histogram_.size(); ++i) {
    size_t alloc_size = 0x1 << i;
    size_t count = allocation_histogram_[i];
    int64 allocation_total =
        static_cast<int64>(alloc_size) * static_cast<int64>(count);

    if (largest_allocation_total < allocation_total) {
      largest_allocation_total = allocation_total;
      largest_allocation_total_idx = i;
    }
  }
  return largest_allocation_total_idx;
}

void AllocationSizeBinner::GetLargestSizeRange(size_t* min_value,
                                               size_t* max_value) const {
  size_t index = GetIndexRepresentingMostMemoryConsumption();
  IndexToSizeRange(index, min_value, max_value);
}

AllocationSizeBinner::AllocationSizeBinner(const AllocationGroup* group_filter)
    : group_filter_(group_filter) {
  allocation_histogram_.resize(33);
}

bool AllocationSizeBinner::PassesFilter(
    const AllocationRecord& alloc_record) const {
  if (group_filter_ == NULL) {
    return true;
  }

  return alloc_record.allocation_group == group_filter_;
}

bool AllocationSizeBinner::Visit(const void* /*memory*/,
                                 const AllocationRecord& alloc_record) {
  if (PassesFilter(alloc_record)) {
    const size_t idx = GetBucketIndexForAllocationSize(alloc_record.size);
    allocation_histogram_[idx]++;
  }
  return true;
}

std::string AllocationSizeBinner::ToCSVString() const {
  size_t first_idx = 0;
  size_t end_idx = allocation_histogram_.size();

  // Determine the start index by skipping all consecutive head entries
  // that are 0.
  while (first_idx < allocation_histogram_.size()) {
    const size_t num_allocs = allocation_histogram_[first_idx];
    if (num_allocs > 0) {
      break;
    }
    first_idx++;
  }

  // Determine the end index by skipping all consecutive tail entries
  // that are 0.
  while (end_idx > 0) {
    if (end_idx < allocation_histogram_.size()) {
      const size_t num_allocs = allocation_histogram_[end_idx];
      if (num_allocs > 0) {
        ++end_idx;
        break;
      }
    }
    end_idx--;
  }

  std::stringstream ss;
  for (size_t i = first_idx; i < end_idx; ++i) {
    size_t min = 0;
    size_t max = 0;
    IndexToSizeRange(i, &min, &max);
    std::stringstream name_ss;
    name_ss << kQuote << min << "..." << max << kQuote;
    ss << name_ss.str() << kDelimiter;
  }
  ss << kNewLine;

  for (size_t i = first_idx; i < end_idx; ++i) {
    const size_t num_allocs = allocation_histogram_[i];
    ss << num_allocs << kDelimiter;
  }
  ss << kNewLine;
  return ss.str();
}

FindTopSizes::FindTopSizes(size_t minimum_size, size_t maximum_size,
                           const AllocationGroup* group)
    : minimum_size_(minimum_size),
      maximum_size_(maximum_size),
      group_filter_(group) {}

bool FindTopSizes::Visit(const void* /*memory*/,
                         const AllocationRecord& alloc_record) {
  if (PassesFilter(alloc_record)) {
    size_counter_[alloc_record.size]++;
  }
  return true;
}

std::string FindTopSizes::ToString(size_t max_elements_to_print) const {
  std::vector<GroupAllocation> group_allocs = GetTopAllocations();
  const size_t n = std::min(max_elements_to_print, group_allocs.size());

  if (!group_allocs.empty()) {
    std::stringstream ss;

    for (size_t i = 0; i < n; ++i) {
      GroupAllocation g = group_allocs[i];
      size_t total_size = g.allocation_count * g.allocation_size;
      ss << "    " << total_size
         << " bytes allocated with object size: " << g.allocation_size
         << " bytes in " << g.allocation_count << " instances " << kNewLine;
    }
    return ss.str();
  } else {
    return std::string();
  }
}

std::vector<FindTopSizes::GroupAllocation> FindTopSizes::GetTopAllocations()
    const {
  std::vector<GroupAllocation> group_allocs;
  // Push objects to a vector.
  for (SizeCounterMap::const_iterator it = size_counter_.begin();
       it != size_counter_.end(); ++it) {
    GroupAllocation alloc = {it->first, it->second};
    group_allocs.push_back(alloc);
  }

  std::sort(group_allocs.begin(), group_allocs.end(),
            GroupAllocation::LessAllocationSize);
  // Biggest first.
  std::reverse(group_allocs.begin(), group_allocs.end());
  return group_allocs;
}

bool FindTopSizes::PassesFilter(const AllocationRecord& alloc_record) const {
  if (alloc_record.size < minimum_size_) return false;
  if (alloc_record.size > maximum_size_) return false;
  if (!group_filter_) return true;  // No group filter when null.
  return group_filter_ == alloc_record.allocation_group;
}

MemoryTrackerLogWriter::MemoryTrackerLogWriter() : start_time_(NowTime()) {
  buffered_file_writer_.reset(new BufferedFileWriter(MemoryLogPath()));
  InitAndRegisterMemoryReporter();
}

MemoryTrackerLogWriter::~MemoryTrackerLogWriter() {
  // No locks are used for the thread reporter, so when it's set to null
  // we allow one second for any suspended threads to run through and finish
  // their reporting.
  SbMemorySetReporter(NULL);
  SbThreadSleep(kSbTimeSecond);
  buffered_file_writer_.reset(NULL);
}

std::string MemoryTrackerLogWriter::tool_name() const {
  return "MemoryTrackerLogWriter";
}

void MemoryTrackerLogWriter::Run(Params* params) {
  // Run function does almost nothing.
  params->logger()->Output("MemoryTrackerLogWriter running...");
}

void MemoryTrackerLogWriter::OnMemoryAllocation(const void* memory_block,
                                                size_t size) {
  void* addresses[kMaxStackSize] = {};
  // Though the SbSystemGetStack API documentation does not specify any possible
  // negative return values, we take no chance.
  const size_t count = std::max(SbSystemGetStack(addresses, kMaxStackSize), 0);

  const size_t n = 256;
  char buff[n] = {0};
  size_t buff_pos = 0;

  int time_since_start_ms = GetTimeSinceStartMs();
  // Writes "+ <ALLOCATION ADDRESS> <size> <time>"
  int bytes_written =
      SbStringFormatF(buff, sizeof(buff), "+ %" PRIXPTR " %x %d",
                      reinterpret_cast<uintptr_t>(memory_block),
                      static_cast<unsigned int>(size), time_since_start_ms);

  buff_pos += bytes_written;
  const size_t end_index = std::min(count, kStartIndex + kNumAddressPrints);

  // For each of the stack addresses that we care about, concat them to the
  // buffer. This was originally written to do multiple stack addresses but
  // this tends to overflow on some lower platforms so it's possible that
  // this loop only iterates once.
  for (size_t i = kStartIndex; i < end_index; ++i) {
    void* p = addresses[i];
    int bytes_written = SbStringFormatF(buff + buff_pos,
                                        sizeof(buff) - buff_pos,
                                        " %" PRIXPTR "",
                                        reinterpret_cast<uintptr_t>(p));
    DCHECK_GE(bytes_written, 0);

    if (bytes_written < 0) {
      DCHECK(false) << "Error occurred while writing string.";
      continue;
    }

    buff_pos += static_cast<size_t>(bytes_written);
  }
  // Adds a "\n" at the end.
  SbStringConcat(buff + buff_pos, "\n", static_cast<int>(n - buff_pos));
  buffered_file_writer_->Append(buff, strlen(buff));
}

void MemoryTrackerLogWriter::OnMemoryDeallocation(const void* memory_block) {
  const size_t n = 256;
  char buff[n] = {0};
  // Writes "- <ADDRESS OF ALLOCATION> \n"
  SbStringFormatF(buff, sizeof(buff), "- %" PRIXPTR "\n",
                  reinterpret_cast<uintptr_t>(memory_block));
  buffered_file_writer_->Append(buff, strlen(buff));
}

void MemoryTrackerLogWriter::OnAlloc(void* context, const void* memory,
                                     size_t size) {
  MemoryTrackerLogWriter* self = static_cast<MemoryTrackerLogWriter*>(context);
  self->OnMemoryAllocation(memory, size);
}

void MemoryTrackerLogWriter::OnDealloc(void* context, const void* memory) {
  MemoryTrackerLogWriter* self = static_cast<MemoryTrackerLogWriter*>(context);
  self->OnMemoryDeallocation(memory);
}

void MemoryTrackerLogWriter::OnMapMemory(void* context, const void* memory,
                                         size_t size) {
  MemoryTrackerLogWriter* self = static_cast<MemoryTrackerLogWriter*>(context);
  self->OnMemoryAllocation(memory, size);
}

void MemoryTrackerLogWriter::OnUnMapMemory(void* context, const void* memory,
                                           size_t size) {
  SB_UNREFERENCED_PARAMETER(size);
  MemoryTrackerLogWriter* self = static_cast<MemoryTrackerLogWriter*>(context);
  self->OnMemoryDeallocation(memory);
}

std::string MemoryTrackerLogWriter::MemoryLogPath() {
  char file_name_buff[2048] = {};
  SbSystemGetPath(kSbSystemPathDebugOutputDirectory, file_name_buff,
                  arraysize(file_name_buff));
  std::string path(file_name_buff);
  if (!path.empty()) {  // Protect against a dangling "/" at end.
    const int back_idx_signed = static_cast<int>(path.length()) - 1;
    if (back_idx_signed >= 0) {
      const size_t idx = back_idx_signed;
      if (path[idx] == '/') {
        path.erase(idx);
      }
    }
  }
  path.append("/memory_log.txt");
  return path;
}

base::TimeTicks MemoryTrackerLogWriter::NowTime() {
  // NowFromSystemTime() is slower but more accurate. However it might
  // be useful to use the faster but less accurate version if there is
  // a speedup.
  return base::TimeTicks::Now();
}

int MemoryTrackerLogWriter::GetTimeSinceStartMs() const {
  base::TimeDelta dt = NowTime() - start_time_;
  return static_cast<int>(dt.InMilliseconds());
}

void MemoryTrackerLogWriter::InitAndRegisterMemoryReporter() {
  DCHECK(!memory_reporter_.get()) << "Memory Reporter already registered.";

  SbMemoryReporter mem_reporter = {OnAlloc, OnDealloc, OnMapMemory,
                                   OnUnMapMemory, this};
  memory_reporter_.reset(new SbMemoryReporter(mem_reporter));
  SbMemorySetReporter(memory_reporter_.get());
}

MemoryTrackerLeakFinder::MemoryTrackerLeakFinder(StackTraceMode mode)
    : string_pool_(128),
      frame_map_(128),
      callframe_map_(128),
      stack_trace_mode_(mode) {
  default_callframe_str_ = &string_pool_.Intern("<Unknown>");
}

MemoryTrackerLeakFinder::~MemoryTrackerLeakFinder() {
  frame_map_.Clear();
  callframe_map_.Clear();
}

bool MemoryTrackerLeakFinder::IsJavascriptScope(
    const nb::analytics::CallStack& callstack) {
  // March through all MemoryScopes in the callstack and check if any of them
  // contains a javascript scope. If it does return true.
  for (nb::analytics::CallStack::const_iterator it = callstack.begin();
       it != callstack.end(); ++it) {
    const NbMemoryScopeInfo* memory_scope = *it;
    const bool is_javascript_scope =
        SbStringFindString(memory_scope->memory_scope_name_, "Javascript");
    if (is_javascript_scope) {
      return true;
    }
  }
  return false;
}

void MemoryTrackerLeakFinder::OnMemoryAllocation(
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

void MemoryTrackerLeakFinder::OnMemoryDeallocation(
    const void* memory_block, const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack) {
  UNREFERENCED_PARAMETER(callstack);

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

std::string MemoryTrackerLeakFinder::tool_name() const {
  return "MemoryTrackerLeakFinder";
}

void MemoryTrackerLeakFinder::Run(Params* params) {
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
        if (alloc_profiles.size() > 20) {
          alloc_profiles.resize(20);
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

const std::string* MemoryTrackerLeakFinder::GetOrCreateSymbol(
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

const std::string* MemoryTrackerLeakFinder::GetOrCreateCplusPlusSymbol(
    const nb::analytics::CallStack& callstack) {
  if (callstack.empty()) {
    return default_callframe_str_;
  } else {
    const NbMemoryScopeInfo* memory_scope = callstack.back();

    const bool skip =
        SbStringFindString(memory_scope->function_name_, "js_malloc") ||
        SbStringFindString(memory_scope->function_name_, "js_realloc") ||
        SbStringFindString(memory_scope->function_name_, "new_");

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

const std::string* MemoryTrackerLeakFinder::TryGetJavascriptSymbol() {
  script::mozjs::util::StackTraceGenerator* js_stack_gen =
      script::mozjs::util::GetThreadLocalStackTraceGenerator();
  if (!js_stack_gen || !js_stack_gen->Valid()) return NULL;

  // Only get one symbol.
  char buffer[256];
  if (!js_stack_gen->GenerateStackTraceString(1, buffer, sizeof(buffer))) {
    return NULL;
  }
  const char* file_name = BaseNameFast(buffer);
  return &string_pool_.Intern(file_name);
}

void MemoryTrackerLeakFinder::SampleSnapshot(
    std::vector<std::pair<const std::string*, AllocRec> >* destination) {
  destination->erase(destination->begin(), destination->end());

  const size_t sample_size = frame_map_.GetSize();

  // Do this locally.
  destination->reserve(sample_size + 10);
  frame_map_.CopyToStdVector(destination);
}

std::string MemoryTrackerLeakFinder::GenerateCSV(
    const std::vector<base::TimeDelta>& time_values,
    const std::vector<AllocationProfile>& data) {
  std::stringstream ss;
  ss << std::fixed;  // Turn off scientific notation for CSV values.
  ss << std::setprecision(3);
  ss << kNewLine << kNewLine;

  // HEADER
  ss << "// Allocation in megabytes." << kNewLine;
  ss << kQuote << "Time(min)" << kQuote << kDelimiter;
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

void MemoryTrackerLeakFinder::GenerateTopLeakingAllocationProfiles(
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
