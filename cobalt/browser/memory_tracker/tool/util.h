// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_UTIL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_UTIL_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/time.h"
#include "starboard/string.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Used for CSV generation.
extern const char kQuote[];
extern const char kDelimiter[];
extern const char kNewLine[];

// This is a simple algorithm to remove the "needle" from the haystack. Note
// that this function is simple and not well optimized.
std::string RemoveString(const std::string& haystack, const char* needle);

// Not optimized but works ok for a tool that dumps out in user time.
std::string SanitizeCSVKey(std::string key);

// Converts "2345.54" => "2,345.54".
std::string InsertCommasIntoNumberString(const std::string& input);

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
  for (size_t i = 0; i * 2 < v->size(); ++i) {
    write_it = v->begin() + i;
    read_it = v->begin() + (i * 2);
    *write_it = *read_it;
  }
  if (write_it != v->end()) {
    write_it++;
  }
  v->erase(write_it, v->end());
}

// Simple timer class that fires periodically after time has elapsed.
class Timer {
 public:
  typedef base::Callback<base::TimeTicks(void)> TimeFunctor;

  explicit Timer(base::TimeDelta delta_time);
  Timer(base::TimeDelta delta_time, TimeFunctor f);

  void Restart();
  bool UpdateAndIsExpired();  // Returns true if the expiration was triggered.
  void ScaleTriggerTime(double scale);

 private:
  base::TimeTicks Now();
  base::TimeTicks start_time_;
  base::TimeDelta time_before_expiration_;
  TimeFunctor time_function_override_;
};

struct AllocationSamples {
  std::vector<int32_t> number_allocations_;
  std::vector<int64_t> allocated_bytes_;
};
typedef std::map<std::string, AllocationSamples> MapAllocationSamples;
typedef std::vector<base::TimeDelta> TimeStamps;

struct TimeSeries {
  MapAllocationSamples samples_;
  TimeStamps time_stamps_;
};

// Generates a linear fit in the form of slope / y-intercept form.
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
    return false;  // Failed to generate any value.
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
const char* BaseNameFast(const char* file_name);

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_UTIL_H_
