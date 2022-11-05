// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_STATISTICS_H_
#define COBALT_BASE_STATISTICS_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "cobalt/base/c_val.h"
#include "starboard/types.h"

namespace base {

namespace internal {

// The default function to convert a sample to its value by dividing.
inline int64_t DefaultSampleToValueFunc(int64_t dividend, int64_t divisor) {
  if (divisor == 0) {
    divisor = 1;
  }
  return dividend / divisor;
}

}  // namespace internal

// Track the statistics of a series of generic samples reprensented as
// dividends and divisors in integer types.  The value of a sample is calculated
// by `SampleToValueFunc` using its dividend and divisor.
//
// Example usages:
//   1. Set dividends to bytes in int and divisors to SbTime, to track the
//      statistics of bandwidth.
//   2. Set the divisor to always be 1, to track the statistics of a count or a
//      duration.
//
// Currently the class can produce the following statistics:
//   1. Average, tracked across all samples.
//   2. Median, tracked across the most recent |MaxSamples| samples.
//   3. Min, tracked across all samples.
//   4. Max, tracked across all samples.
//
// Notes:
//   1. The accumulated values are stored in `int64_t`, and it is the user's
//      responsibility to avoid overflow.
//   2. The class isn't multi-thread safe, and it is the user's responsibility
//      to synchronize the usage when necessary.
//   3. Both the dividend and divisor of the samples should ideally be positive,
//      which is NOT enforced by the class.

#if defined(COBALT_BUILD_TYPE_GOLD)

template <typename DividendType, typename DivisorType, int MaxSamples,
          int64_t (*SampleToValueFunc)(int64_t, int64_t) =
              internal::DefaultSampleToValueFunc>
class Statistics {
 public:
  explicit Statistics(const std::string& metric_name) {}

  void AddSample(DividendType dividend, DivisorType divisor) {}
  int64_t accumulated_dividend() const { return 0; }
  int64_t accumulated_divisor() const { return 1; }
  int64_t average() const { return 0; }
  int64_t min() const { return 0; }
  int64_t max() const { return 0; }

  int64_t GetMedian() const { return 0; }
};

#else  // defined(COBALT_BUILD_TYPE_GOLD)

template <typename DividendType, typename DivisorType, size_t MaxSamples,
          int64_t (*SampleToValueFunc)(int64_t, int64_t) =
              internal::DefaultSampleToValueFunc>
class Statistics {
 public:
  explicit Statistics(const std::string& metric_name)
      : last_sample_value_(metric_name + ".Latest", 0,
                           metric_name + " most recent value."),
        median_sample_value_(metric_name + ".Median", 0,
                             metric_name + " median value.") {}

  void AddSample(DividendType dividend, DivisorType divisor) {
    static_assert(MaxSamples > 0, "MaxSamples has to be greater than 0.");

    auto& current =
        samples_[(first_sample_index_ + number_of_samples_) % MaxSamples];
    if (number_of_samples_ == MaxSamples) {
      first_sample_index_ = (first_sample_index_ + 1) % MaxSamples;
    } else {
      ++number_of_samples_;
    }

    current.dividend = dividend;
    current.divisor = divisor;
    accumulated_dividend_ += dividend;
    accumulated_divisor_ += divisor;

    auto value = GetSampleValue(current);
    if (first_sample_added_) {
      min_ = std::min(min_, value);
      max_ = std::max(max_, value);
    } else {
      min_ = max_ = value;
      first_sample_added_ = true;
    }
    last_sample_value_ = value;
    // TODO(b/258531018) this should be optimized in future.
    median_sample_value_ = GetMedian();
  }

  int64_t accumulated_dividend() const { return accumulated_dividend_; }
  int64_t accumulated_divisor() const { return accumulated_divisor_; }

  int64_t average() const {
    return SampleToValueFunc(accumulated_dividend_, accumulated_divisor_);
  }
  int64_t min() const { return min_; }
  int64_t max() const { return max_; }

  // When there are even number of samples in the object, it is implementation
  // specific to pick any number close to the middle, or the median of the
  // numbers close to the middle.  Use {1, 2, 3, 4} as an example, the median
  // can be 3, or 4, or 3.5.
  int64_t GetMedian() const {
    if (number_of_samples_ == 0) {
      return 0;
    }

    std::vector<Sample> copy;
    copy.reserve(number_of_samples_);
    if (first_sample_index_ + number_of_samples_ <= MaxSamples) {
      copy.assign(samples_ + first_sample_index_,
                  samples_ + first_sample_index_ + number_of_samples_);
    } else {
      auto samples_to_copy = number_of_samples_;
      copy.assign(samples_ + first_sample_index_, samples_ + MaxSamples);
      samples_to_copy -= MaxSamples - first_sample_index_;
      copy.insert(copy.end(), samples_, samples_ + samples_to_copy);
    }

    std::nth_element(copy.begin(), copy.begin() + number_of_samples_ / 2,
                     copy.end(), [](const Sample& left, const Sample& right) {
                       return GetSampleValue(left) < GetSampleValue(right);
                     });
    return GetSampleValue(copy[number_of_samples_ / 2]);
  }

 private:
  Statistics(const Statistics&) = delete;
  Statistics& operator=(const Statistics&) = delete;

  struct Sample {
    DividendType dividend = 0;
    DivisorType divisor = 0;
  };

  static int64_t GetSampleValue(const Sample& sample) {
    return SampleToValueFunc(static_cast<int64_t>(sample.dividend),
                             static_cast<int64_t>(sample.divisor));
  }

  bool first_sample_added_ = false;

  int64_t accumulated_dividend_ = 0;
  int64_t accumulated_divisor_ = 0;
  int64_t min_ = 0;
  int64_t max_ = 0;

  Sample samples_[MaxSamples] = {};  // Ring buffer for samples.
  size_t number_of_samples_ = 0;     // Number of samples in |samples_|.
  size_t first_sample_index_ = 0;    // Index of the first sample in |samples_|.

  base::CVal<int64_t> last_sample_value_;
  base::CVal<int64_t> median_sample_value_;
};

#endif  // defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace base

#endif  // COBALT_BASE_STATISTICS_H_
