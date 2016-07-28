// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class StatisticsRecorderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    InitializeStatisticsRecorder();
  }

  virtual void TearDown() {
    UninitializeStatisticsRecorder();
  }

  void InitializeStatisticsRecorder() {
    statistics_recorder_ = new StatisticsRecorder();
  }

  void UninitializeStatisticsRecorder() {
    delete statistics_recorder_;
    statistics_recorder_ = NULL;
  }

  void DeleteHistogram(Histogram* histogram) {
    delete histogram;
  }

  StatisticsRecorder* statistics_recorder_;
};

TEST_F(StatisticsRecorderTest, NotInitialized) {
  UninitializeStatisticsRecorder();

  ASSERT_FALSE(StatisticsRecorder::IsActive());

  StatisticsRecorder::Histograms registered_histograms;
  std::vector<const BucketRanges*> registered_ranges;

  // We can still create histograms, but it's not registered.
  // TODO(kaiwang): Do not depend on Histogram FactoryGet implementation.
  Histogram* histogram(
      Histogram::FactoryGet("StatisticsRecorderTest_NotInitialized",
                            1, 1000, 10, Histogram::kNoFlags));
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(0u, registered_histograms.size());

  // RegisterOrDeleteDuplicateRanges is a no-op.
  BucketRanges* ranges = new BucketRanges(3);;
  ranges->ResetChecksum();
  EXPECT_EQ(ranges,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges));
  StatisticsRecorder::GetBucketRanges(&registered_ranges);
  EXPECT_EQ(0u, registered_ranges.size());

  DeleteHistogram(histogram);
}

TEST_F(StatisticsRecorderTest, RegisterBucketRanges) {
  std::vector<const BucketRanges*> registered_ranges;

  BucketRanges* ranges1 = new BucketRanges(3);;
  ranges1->ResetChecksum();
  BucketRanges* ranges2 = new BucketRanges(4);;
  ranges2->ResetChecksum();

  // Register new ranges.
  EXPECT_EQ(ranges1,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges1));
  EXPECT_EQ(ranges2,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges2));
  StatisticsRecorder::GetBucketRanges(&registered_ranges);
  ASSERT_EQ(2u, registered_ranges.size());

  // Register some ranges again.
  EXPECT_EQ(ranges1,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges1));
  registered_ranges.clear();
  StatisticsRecorder::GetBucketRanges(&registered_ranges);
  ASSERT_EQ(2u, registered_ranges.size());
  // Make sure the ranges is still the one we know.
  ASSERT_EQ(3u, ranges1->size());
  EXPECT_EQ(0, ranges1->range(0));
  EXPECT_EQ(0, ranges1->range(1));
  EXPECT_EQ(0, ranges1->range(2));

  // Register ranges with same values.
  BucketRanges* ranges3 = new BucketRanges(3);;
  ranges3->ResetChecksum();
  EXPECT_EQ(ranges1,  // returning ranges1
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges3));
  registered_ranges.clear();
  StatisticsRecorder::GetBucketRanges(&registered_ranges);
  ASSERT_EQ(2u, registered_ranges.size());
}

TEST_F(StatisticsRecorderTest, RegisterHistogram) {
  // Create a Histogram that was not registered.
  // TODO(kaiwang): Do not depend on Histogram FactoryGet implementation.
  UninitializeStatisticsRecorder();
  Histogram* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags);

  // Clean StatisticsRecorder.
  InitializeStatisticsRecorder();
  StatisticsRecorder::Histograms registered_histograms;
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(0u, registered_histograms.size());

  // Register the Histogram.
  EXPECT_EQ(histogram,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram));
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(1u, registered_histograms.size());

  // Register the same Histogram again.
  EXPECT_EQ(histogram,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram));
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(1u, registered_histograms.size());
}

TEST_F(StatisticsRecorderTest, FindHistogram) {
  Histogram* histogram1 = Histogram::FactoryGet(
      "TestHistogram1", 1, 1000, 10, Histogram::kNoFlags);
  Histogram* histogram2 = Histogram::FactoryGet(
      "TestHistogram2", 1, 1000, 10, Histogram::kNoFlags);

  EXPECT_EQ(histogram1, StatisticsRecorder::FindHistogram("TestHistogram1"));
  EXPECT_EQ(histogram2, StatisticsRecorder::FindHistogram("TestHistogram2"));
  EXPECT_TRUE(StatisticsRecorder::FindHistogram("TestHistogram") == NULL);
}

TEST_F(StatisticsRecorderTest, GetSnapshot) {
  Histogram::FactoryGet("TestHistogram1", 1, 1000, 10, Histogram::kNoFlags);
  Histogram::FactoryGet("TestHistogram2", 1, 1000, 10, Histogram::kNoFlags);
  Histogram::FactoryGet("TestHistogram3", 1, 1000, 10, Histogram::kNoFlags);

  StatisticsRecorder::Histograms snapshot;
  StatisticsRecorder::GetSnapshot("Test", &snapshot);
  EXPECT_EQ(3u, snapshot.size());

  snapshot.clear();
  StatisticsRecorder::GetSnapshot("1", &snapshot);
  EXPECT_EQ(1u, snapshot.size());

  snapshot.clear();
  StatisticsRecorder::GetSnapshot("hello", &snapshot);
  EXPECT_EQ(0u, snapshot.size());
}

TEST_F(StatisticsRecorderTest, RegisterHistogramWithFactoryGet) {
  StatisticsRecorder::Histograms registered_histograms;

  StatisticsRecorder::GetHistograms(&registered_histograms);
  ASSERT_EQ(0u, registered_histograms.size());

  // Create a Histogram.
  Histogram* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(1u, registered_histograms.size());

  // Get an existing histogram.
  Histogram* histogram2 = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(1u, registered_histograms.size());
  EXPECT_EQ(histogram, histogram2);

  // Create a LinearHistogram.
  histogram = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, Histogram::kNoFlags);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(2u, registered_histograms.size());

  // Create a BooleanHistogram.
  histogram = BooleanHistogram::FactoryGet(
      "TestBooleanHistogram", Histogram::kNoFlags);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(3u, registered_histograms.size());

  // Create a CustomHistogram.
  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, Histogram::kNoFlags);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(4u, registered_histograms.size());
}

TEST_F(StatisticsRecorderTest, RegisterHistogramWithMacros) {
  StatisticsRecorder::Histograms registered_histograms;

  Histogram* histogram = Histogram::FactoryGet(
      "TestHistogramCounts", 1, 1000000, 50, Histogram::kNoFlags);

  // The histogram we got from macro is the same as from FactoryGet.
  HISTOGRAM_COUNTS("TestHistogramCounts", 30);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  ASSERT_EQ(1u, registered_histograms.size());
  EXPECT_EQ(histogram, registered_histograms[0]);

  HISTOGRAM_TIMES("TestHistogramTimes", TimeDelta::FromDays(1));
  HISTOGRAM_ENUMERATION("TestHistogramEnumeration", 20, 200);

  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
  EXPECT_EQ(3u, registered_histograms.size());

  // Debugging only macros.
  DHISTOGRAM_TIMES("TestHistogramDebugTimes", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("TestHistogramDebugCounts", 30);
  registered_histograms.clear();
  StatisticsRecorder::GetHistograms(&registered_histograms);
#ifndef NDEBUG
  EXPECT_EQ(5u, registered_histograms.size());
#else
  EXPECT_EQ(3u, registered_histograms.size());
#endif
}

TEST_F(StatisticsRecorderTest, BucketRangesSharing) {
  Histogram* histogram1(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));
  Histogram* histogram2(Histogram::FactoryGet(
      "Histogram2", 1, 64, 8, Histogram::kNoFlags));
  Histogram* histogram3(Histogram::FactoryGet(
      "Histogram3", 1, 64, 16, Histogram::kNoFlags));

  const BucketRanges* bucket_ranges1 = histogram1->bucket_ranges();
  const BucketRanges* bucket_ranges2 = histogram2->bucket_ranges();
  const BucketRanges* bucket_ranges3 = histogram3->bucket_ranges();
  EXPECT_EQ(bucket_ranges1, bucket_ranges2);
  EXPECT_FALSE(bucket_ranges1->Equals(bucket_ranges3));
}

}  // namespace base
