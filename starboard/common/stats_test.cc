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

#include "starboard/common/stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(StatsTest, AddSample) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  ASSERT_EQ(stats.size(), 3);
}

TEST(StatsTest, Size) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  ASSERT_EQ(stats.size(), 2);
}

TEST(StatsTest, Mean) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  ASSERT_EQ(stats.mean(), 2);
}

TEST(StatsTest, Min) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  ASSERT_EQ(stats.min(), 1);
}

TEST(StatsTest, Max) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  ASSERT_EQ(stats.max(), 3);
}

TEST(StatsTest, End) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  auto it = stats.end();
  ASSERT_EQ(*(--it), 2);
}

TEST(StatsTest, AddSampleWithWraparound) {
  Stats<int, 3> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  stats.addSample(4);
  ASSERT_EQ(stats.size(), 3);
  ASSERT_EQ(stats.min(), 2);
  ASSERT_EQ(stats.max(), 4);
}

TEST(StatsTest, MeanWithEmptyStats) {
  Stats<int, 5> stats;
  ASSERT_EQ(stats.mean(), 0);
}

TEST(StatsTest, VarianceWithEmptyStats) {
  Stats<int, 5> stats;
  ASSERT_EQ(stats.variance(), 0);
}

TEST(StatsTest, StdDevWithEmptyStats) {
  Stats<int, 5> stats;
  ASSERT_EQ(stats.stdDev(), 0);
}

TEST(StatsTest, MedianWithEmptyStats) {
  Stats<int, 5> stats;
  ASSERT_EQ(stats.median(), 0);
}

TEST(StatsTest, MedianWithOddSize) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  stats.addSample(4);
  stats.addSample(5);
  ASSERT_EQ(stats.median(), 3);
}

TEST(StatsTest, MedianWithEvenSize) {
  Stats<int, 6> stats;
  stats.addSample(10);
  stats.addSample(20);
  stats.addSample(30);
  stats.addSample(40);
  stats.addSample(50);
  stats.addSample(60);
  ASSERT_EQ(stats.median(), 35);
}

TEST(StatsTest, Variance) {
  Stats<int, 5> stats;
  stats.addSample(10);
  stats.addSample(20);
  stats.addSample(30);
  ASSERT_EQ(stats.variance(), 66);
}

TEST(StatsTest, StdDev) {
  Stats<int, 5> stats;
  stats.addSample(10);
  stats.addSample(20);
  stats.addSample(30);
  ASSERT_EQ(stats.stdDev(), 8);
}

TEST(StatsTest, HistogramBins) {
  Stats<int, 5> stats;
  stats.addSample(10);
  stats.addSample(20);
  stats.addSample(30);
  stats.addSample(40);
  stats.addSample(50);
  std::vector<std::tuple<int, int, size_t>> histogram = stats.histogramBins(3);
  ASSERT_EQ(histogram.size(), 3);
  ASSERT_EQ(std::get<0>(histogram[0]), 10);
  ASSERT_EQ(std::get<1>(histogram[0]), 23);
  ASSERT_EQ(std::get<2>(histogram[0]), 2);  // 10, 20
  ASSERT_EQ(std::get<0>(histogram[1]), 23);
  ASSERT_EQ(std::get<1>(histogram[1]), 36);
  ASSERT_EQ(std::get<2>(histogram[1]), 1);  // 30
  ASSERT_EQ(std::get<0>(histogram[2]), 36);
  ASSERT_EQ(std::get<1>(histogram[2]), 50);
  ASSERT_EQ(std::get<2>(histogram[2]), 2);  // 40, 50
}

TEST(StatsTest, HistogramBinsEmpty) {
  Stats<int, 5> stats;
  std::vector<std::tuple<int, int, size_t>> expectedHistogram;
  ASSERT_EQ(stats.histogramBins(5), expectedHistogram);
}

TEST(StatsTest, HistogramBinsZeroBins) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  std::vector<std::tuple<int, int, size_t>> expectedHistogram;
  ASSERT_EQ(stats.histogramBins(0), expectedHistogram);
}

TEST(StatsTest, HistogramBinsOneBin) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  std::vector<std::tuple<int, int, size_t>> expectedHistogram = {
      std::make_tuple(1, 3, 3)};
  ASSERT_EQ(stats.histogramBins(1), expectedHistogram);
}

TEST(StatsTest, HistogramBinsMultipleBins) {
  Stats<int, 5> stats;
  stats.addSample(1);
  stats.addSample(2);
  stats.addSample(3);
  stats.addSample(4);
  stats.addSample(5);
  std::vector<std::tuple<int, int, size_t>> expectedHistogram = {
      std::make_tuple(1, 2, 1), std::make_tuple(2, 3, 1),
      std::make_tuple(3, 4, 1), std::make_tuple(4, 5, 2)};
  ASSERT_EQ(stats.histogramBins(4), expectedHistogram);
}

}  // namespace
}  // namespace starboard
