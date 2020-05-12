// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_BASE_C_VAL_TIME_INTERVAL_ENTRY_STATS_H_
#define COBALT_BASE_C_VAL_TIME_INTERVAL_ENTRY_STATS_H_

#include <algorithm>
#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cobalt/base/c_val.h"

namespace base {
namespace CValDetail {

// This class tracks entries over a specified time period, which it does not
// retain in memory. When the time period has elapsed, the average, minimum,
// maximum, and standard deviation of that time period are recorded with CVals,
// and the tracking resets for the next time period.
// NOTE: In cases where the number of entries are small and stats for time
//       periods are not needed, |CValCollectionEntryStats| may be more
//       appropriate. It retains the entries for the collection in memory so
//       that it can provide the collection's percentiles.
template <typename EntryType, typename Visibility>
class CValTimeIntervalEntryStatsImpl {
 public:
  CValTimeIntervalEntryStatsImpl(const std::string& name,
                                 int64 time_interval_in_ms);

  void AddEntry(const EntryType& value);
  void AddEntry(const EntryType& value, const base::TimeTicks& now);

 private:
  void AddToActiveEntryStats(const EntryType& value);
  void ResetActiveEntryStats();

  const int64 time_interval_in_ms_;

  // CVals of the stats for the previously completed time interval.
  base::CVal<size_t, Visibility> count_;
  base::CVal<EntryType, Visibility> average_;
  base::CVal<EntryType, Visibility> minimum_;
  base::CVal<EntryType, Visibility> maximum_;
  base::CVal<EntryType, Visibility> standard_deviation_;

  // Active time interval-related
  base::TimeTicks active_start_time_;

  // Variance calculations are based upon the following:
  // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Computing_shifted_data
  // Each value is shifted by an estimated mean when calculating the variance.
  // This increases accuracy in cases where the variance is small but the values
  // involved are extremely large.
  // The following article provides a more detailed discussion on the topic:
  // http://www.cs.yale.edu/publications/techreports/tr222.pdf
  // During the first time interval, the initial entry acts as the estimated
  // mean. All subsequent loops use the mean of the previous time interval.
  double active_estimated_mean_;

  // Stats updated with each new entry.
  int64 active_count_;
  double active_shifted_sum_;
  double active_shifted_sum_squares_;
  EntryType active_minimum_;
  EntryType active_maximum_;
};

template <typename EntryType, typename Visibility>
CValTimeIntervalEntryStatsImpl<EntryType, Visibility>::
    CValTimeIntervalEntryStatsImpl(const std::string& name,
                                   int64 time_interval_in_ms)
    : time_interval_in_ms_(time_interval_in_ms),
      count_(base::StringPrintf("%s.Cnt", name.c_str()), 0, "Total entries."),
      average_(base::StringPrintf("%s.Avg", name.c_str()), EntryType(),
               "Average time."),
      minimum_(base::StringPrintf("%s.Min", name.c_str()), EntryType(),
               "Minimum time."),
      maximum_(base::StringPrintf("%s.Max", name.c_str()), EntryType(),
               "Maximum time."),
      standard_deviation_(base::StringPrintf("%s.Std", name.c_str()),
                          EntryType(), "Standard deviation of times."),
      active_estimated_mean_(0) {
  ResetActiveEntryStats();
}

template <typename EntryType, typename Visibility>
void CValTimeIntervalEntryStatsImpl<EntryType, Visibility>::AddEntry(
    const EntryType& value) {
  base::TimeTicks now = base::TimeTicks::Now();
  AddEntry(value, now);
}

template <typename EntryType, typename Visibility>
void CValTimeIntervalEntryStatsImpl<EntryType, Visibility>::AddEntry(
    const EntryType& value, const base::TimeTicks& now) {
  // Check to see if the timer hasn't started yet. This happens when this is
  // the first entry ever added. In this case, the timer is simply started and
  // the estimated mean is set to the passed in value.
  if (active_start_time_.is_null()) {
    active_start_time_ = now;
    active_estimated_mean_ = CValDetail::ToDouble(value);
    // Otherwise, check for the time interval having ended. If it has, then the
    // CVals are updated using the active stats, the active stats are reset, and
    // the timer restarted.
  } else if ((now - active_start_time_).InMilliseconds() >
             time_interval_in_ms_) {
    // The active count can never be 0 when a time interval has ended. There
    // must always be at least one entry when the time is non-null.
    DCHECK_GT(active_count_, 0);
    count_ = active_count_;

    double active_shifted_mean = active_shifted_sum_ / active_count_;
    average_ = CValDetail::FromDouble<EntryType>(active_estimated_mean_ +
                                                 active_shifted_mean);
    // The equation comes from the following:
    // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Computing_shifted_data
    double variance =
        (active_shifted_sum_squares_ -
         ((active_shifted_sum_ * active_shifted_sum_) / active_count_)) /
        active_count_;
    variance = std::max(variance, 0.0);
    standard_deviation_ =
        CValDetail::FromDouble<EntryType>(std::sqrt(variance));
    minimum_ = active_minimum_;
    maximum_ = active_maximum_;

    // Prepare the active data for the next time interval, including updating
    // the estimated mean with the calculated mean of the previous time
    // interval.
    active_start_time_ = now;
    active_estimated_mean_ += active_shifted_mean;
    ResetActiveEntryStats();
  }

  AddToActiveEntryStats(value);
}

template <typename EntryType, typename Visibility>
void CValTimeIntervalEntryStatsImpl<
    EntryType, Visibility>::AddToActiveEntryStats(const EntryType& value) {
  ++active_count_;

  double shifted_value_as_double =
      CValDetail::ToDouble(value) - active_estimated_mean_;
  active_shifted_sum_ += shifted_value_as_double;
  active_shifted_sum_squares_ +=
      shifted_value_as_double * shifted_value_as_double;

  if (value < active_minimum_) {
    active_minimum_ = value;
  }
  if (value > active_maximum_) {
    active_maximum_ = value;
  }
}

template <typename EntryType, typename Visibility>
void CValTimeIntervalEntryStatsImpl<EntryType,
                                    Visibility>::ResetActiveEntryStats() {
  active_count_ = 0;

  active_shifted_sum_ = 0;
  active_shifted_sum_squares_ = 0;

  active_minimum_ = CValDetail::Max<EntryType>();
  active_maximum_ = CValDetail::Min<EntryType>();
}

#if !defined(ENABLE_DEBUG_C_VAL)
// This is a stub class that disables CValTimeIntervalEntryStats when
// ENABLE_DEBUG_C_VAL is not defined.
template <typename EntryType>
class CValTimeIntervalEntryStatsStub {
 public:
  CValTimeIntervalEntryStatsStub(const std::string& name,
                                 int64 time_interval_in_ms) {
  }

  void AddEntry(const EntryType& value) {}
  void AddEntry(const EntryType& value, const base::TimeTicks& now) {}
};
#endif  // ENABLE_DEBUG_C_VAL

}  // namespace CValDetail

template <typename EntryType, typename Visibility = CValDebug>
class CValTimeIntervalEntryStats {};

template <typename EntryType>
#if defined(ENABLE_DEBUG_C_VAL)
// When ENABLE_DEBUG_C_VAL is defined, CVals with Visibility set to CValDebug
// are tracked through the CVal system.
class CValTimeIntervalEntryStats<EntryType, CValDebug>
    : public CValDetail::CValTimeIntervalEntryStatsImpl<EntryType, CValDebug> {
  typedef CValDetail::CValTimeIntervalEntryStatsImpl<EntryType, CValDebug>
      CValParent;
#else   // ENABLE_DEBUG_C_VAL
// When ENABLE_DEBUG_C_VAL is not defined, CVals with Visibility set to
// CValDebug are not tracked though the CVal system and
// CValTimeIntervalEntryStats can be stubbed out.
class CValTimeIntervalEntryStats<EntryType, CValDebug>
    : public CValDetail::CValTimeIntervalEntryStatsStub<EntryType> {
  typedef CValDetail::CValTimeIntervalEntryStatsStub<EntryType> CValParent;
#endif  // ENABLE_DEBUG_C_VAL

 public:
  CValTimeIntervalEntryStats(const std::string& name, int64 time_interval_in_ms)
      : CValParent(name, time_interval_in_ms) {}
};

// CVals with visibility set to CValPublic are always tracked though the CVal
// system, regardless of the ENABLE_DEBUG_C_VAL state.
template <typename EntryType>
class CValTimeIntervalEntryStats<EntryType, CValPublic>
    : public CValDetail::CValTimeIntervalEntryStatsImpl<EntryType, CValPublic> {
  typedef CValDetail::CValTimeIntervalEntryStatsImpl<EntryType, CValPublic>
      CValParent;

 public:
  CValTimeIntervalEntryStats(const std::string& name, int64 time_interval_in_ms)
      : CValParent(name, time_interval_in_ms) {}
};

}  // namespace base

#endif  // COBALT_BASE_C_VAL_TIME_INTERVAL_ENTRY_STATS_H_
