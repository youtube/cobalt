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

#include "cobalt/base/statistics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(StatisticsTest, ZeroSamples) {
  Statistics<int, int, 1> statistics("test1");

  EXPECT_EQ(statistics.accumulated_dividend(), 0);
  EXPECT_EQ(statistics.accumulated_divisor(), 0);
  EXPECT_EQ(statistics.average(), 0);
  EXPECT_EQ(statistics.min(), 0);
  EXPECT_EQ(statistics.max(), 0);
  EXPECT_EQ(statistics.GetMedian(), 0);
}

TEST(StatisticsTest, SingleSample) {
  Statistics<int, int, 1> statistics("test2");

  statistics.AddSample(100, 10);

  EXPECT_EQ(statistics.accumulated_dividend(), 100);
  EXPECT_EQ(statistics.accumulated_divisor(), 10);
  EXPECT_EQ(statistics.average(), 10);
  EXPECT_EQ(statistics.min(), 10);
  EXPECT_EQ(statistics.max(), 10);
  EXPECT_EQ(statistics.GetMedian(), 10);
}

TEST(StatisticsTest, NotCrashOnZeroDivisor) {
  Statistics<int, int, 4> statistics("test3");

  statistics.AddSample(100, 0);

  EXPECT_EQ(statistics.accumulated_dividend(), 100);
  EXPECT_EQ(statistics.accumulated_divisor(), 0);
  // The only expectation for the following calls is that they don't crash.
  statistics.average();
  statistics.min();
  statistics.max();
  statistics.GetMedian();
}

TEST(StatisticsTest, MultipleSamples) {
  Statistics<int, int, 4> statistics("test4");

  statistics.AddSample(10, 2);
  statistics.AddSample(20, 2);
  statistics.AddSample(30, 2);
  EXPECT_EQ(statistics.accumulated_dividend(), 10 + 20 + 30);
  EXPECT_EQ(statistics.accumulated_divisor(), 6);
  EXPECT_EQ(statistics.average(), (10 + 20 + 30) / 6);
  EXPECT_EQ(statistics.min(), 10 / 2);
  EXPECT_EQ(statistics.max(), 30 / 2);
  EXPECT_EQ(statistics.GetMedian(), 20 / 2);

  statistics.AddSample(40, 2);
  EXPECT_EQ(statistics.accumulated_dividend(), 10 + 20 + 30 + 40);
  EXPECT_EQ(statistics.accumulated_divisor(), 8);
  EXPECT_EQ(statistics.average(), (10 + 20 + 30 + 40) / 8);
  EXPECT_EQ(statistics.min(), 10 / 2);
  EXPECT_EQ(statistics.max(), 40 / 2);
  // The median can be in the range of either number close to the middle, and is
  // implementation specific.
  EXPECT_GE(statistics.GetMedian(), 30 / 2);
  EXPECT_LE(statistics.GetMedian(), 40 / 2);
}

TEST(StatisticsTest, MultipleSamplesDifferentOrders) {
  Statistics<int, int, 10> statistics("test5");
  Statistics<int, int, 10> statistics_reversed("reverse test5");

  for (int i = 0; i < 10; ++i) {
    statistics.AddSample(i * i, i * 3);
  }

  for (int i = 9; i >= 0; --i) {
    statistics_reversed.AddSample(i * i, i * 3);
  }

  EXPECT_EQ(statistics.accumulated_dividend(),
            statistics_reversed.accumulated_dividend());
  EXPECT_EQ(statistics.accumulated_divisor(),
            statistics_reversed.accumulated_divisor());
  EXPECT_EQ(statistics.average(), statistics_reversed.average());
  EXPECT_EQ(statistics.min(), statistics_reversed.min());
  EXPECT_EQ(statistics.max(), statistics_reversed.max());
  EXPECT_EQ(statistics.GetMedian(), statistics_reversed.GetMedian());
}

TEST(StatisticsTest, MoreSamplesThanCapacity) {
  Statistics<int, int, 1> statistics_1("test6");
  Statistics<int, int, 10> statistics_10("test7");

  for (int i = 0; i < 5; ++i) {
    statistics_1.AddSample(i * i, i * 3);
    statistics_10.AddSample(i * i, i * 3);
  }

  Statistics<int, int, 1> statistics_1_median_reference("test8");
  Statistics<int, int, 10> statistics_10_median_reference("test9");

  for (int i = 0; i < 10; ++i) {
    statistics_1.AddSample(i * i * i, i * 5);
    statistics_10.AddSample(i * i * i, i * 5);
    statistics_1_median_reference.AddSample(i * i * i, i * 5);
    statistics_10_median_reference.AddSample(i * i * i, i * 5);
  }

  // All statistics except median are tracked over all samples added, median is
  // tracked on the samples cached, so:
  // 1. All statistics except median are the same over `statistics_1` and
  // `statistics_10`.
  // 2. Medians of `statistics_1` and `statistics_1_median_reference` are the
  //    same, and the same to `statistics_10` and
  //    `statistics_10_median_reference`.
  EXPECT_EQ(statistics_1.accumulated_dividend(),
            statistics_10.accumulated_dividend());
  EXPECT_EQ(statistics_1.accumulated_divisor(),
            statistics_10.accumulated_divisor());
  EXPECT_EQ(statistics_1.average(), statistics_10.average());
  EXPECT_EQ(statistics_1.min(), statistics_10.min());
  EXPECT_EQ(statistics_1.max(), statistics_10.max());

  EXPECT_EQ(statistics_1.GetMedian(),
            statistics_1_median_reference.GetMedian());
  EXPECT_EQ(statistics_10.GetMedian(),
            statistics_10_median_reference.GetMedian());
}

TEST(StatisticsTest, MedianWithOverflow) {
  Statistics<int, int, 3> statistics("test10");

  statistics.AddSample(1, 1);
  statistics.AddSample(11, 1);
  statistics.AddSample(21, 1);
  EXPECT_EQ(statistics.GetMedian(), 11);

  // Removed 1, with {11, 21, 16} cached.
  statistics.AddSample(16, 1);
  EXPECT_EQ(statistics.GetMedian(), 16);

  // Removed 11, with {21, 16, 26} cached.
  statistics.AddSample(26, 1);
  EXPECT_EQ(statistics.GetMedian(), 21);
}

}  // namespace
}  // namespace base
