// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Stats module C++ convenience layer
//
// Implements a simple statistics class that can be used to calculate the
// minimum, maximum, mean, variance, standard deviation, median, and histogram
// of a set of samples held in a ring buffer.
// Note: all arithmetic operations are performed using the T type, so the
// precision of the results is determined by the precision of the T type.

#ifndef STARBOARD_COMMON_STATS_H_
#define STARBOARD_COMMON_STATS_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <tuple>
#include <vector>

namespace starboard {

template <typename T, size_t N>
class Stats {
  static_assert(N > 0, "N must be greater than 0.");
  static_assert(std::is_arithmetic<T>::value, "Requires arithmetic types.");

 public:
  void addSample(const T& t) {
    samples[head++] = t;
    head = head % N;
    wrapped = wrapped || (head == 0);
  }
  size_t size() const { return wrapped ? N : head; }
  T min() const { return *std::min_element(samples.begin(), end()); }

  T max() const { return *std::max_element(samples.begin(), end()); }

  T mean() const {
    if (size() == 0)
      return T();
    return accumulate(samples.begin(), end(), T(0), std::plus<>{}) / size();
  }

  T variance() const {
    if (size() < 2)
      return T();

    T m = mean();
    auto var_func = [m](const T acc, const T& n) {
      return acc + (n - m) * (n - m);
    };
    T var = accumulate(samples.begin(), end(), T(0), var_func) / size();
    return var;
  }
  T stdDev() const { return std::sqrt(variance()); }
  T median() const {
    if (size() == 0)
      return T();

    std::vector<T> sorted_samples(samples.begin(), end());
    std::sort(sorted_samples.begin(), sorted_samples.end());

    size_t n = sorted_samples.size();
    if (n % 2 == 0) {
      return (sorted_samples[n / 2 - 1] + sorted_samples[n / 2]) / T(2);
    } else {
      return sorted_samples[n / 2];
    }
  }
  typename std::array<T, N>::const_iterator end() const {
    return wrapped ? samples.end() : samples.begin() + head;
  }

  // Returns a vector of tuples, each containing the lower bound, upper bound,
  // and count of a bin
  std::vector<std::tuple<T, T, size_t>> histogramBins(
      size_t numberOfBins) const {
    return histogramBins(numberOfBins, min(), max());
  }

  // Returns a vector of tuples, each containing the lower bound, upper bound,
  // and count of a bin
  std::vector<std::tuple<T, T, size_t>> histogramBins(size_t numberOfBins,
                                                      T minValue,
                                                      T maxValue) const {
    return createHistogram(numberOfBins, minValue, maxValue);
  }

 private:
  // Common method used to generate histogram bins
  std::vector<std::tuple<T, T, size_t>> createHistogram(size_t numberOfBins,
                                                        T minValue,
                                                        T maxValue) const {
    std::vector<std::tuple<T, T, size_t>> histogram;
    if (size() == 0 || numberOfBins == 0 || minValue == maxValue) {
      return histogram;
    }

    T range = maxValue - minValue;
    T binWidth = range / numberOfBins;

    // Initialize bins
    for (size_t i = 0; i < numberOfBins; ++i) {
      histogram.emplace_back(minValue + i * binWidth,
                             minValue + (i + 1) * binWidth, 0);
    }
    std::get<1>(histogram.back()) =
        maxValue;  // Adjust the last bin to include maxValue

    // Count samples in each bin
    for (auto it = samples.begin(); it != end(); ++it) {
      if (*it >= minValue &&
          *it <= maxValue) {  // Include samples within specified range
        size_t binIndex =
            std::min(numberOfBins - 1,
                     static_cast<size_t>(((*it - minValue) / binWidth)));
        std::get<2>(histogram[binIndex])++;
      }
    }

    return histogram;
  }

  template <typename InputIt, typename TAcc, typename BinaryOperation>
  TAcc accumulate(InputIt first,
                  InputIt last,
                  TAcc init,
                  BinaryOperation op) const {
#if __cplusplus >= 202002L
    return std::accumulate(first, last, init, op);
#else
    for (; first != last; ++first) {
      init = op(init, *first);
    }
    return init;
#endif
  }

  bool wrapped = false;
  size_t head = 0;
  std::array<T, N> samples;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_STATS_H_
