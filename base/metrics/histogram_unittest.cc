// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of Histogram class

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;

namespace base {

// Check for basic syntax and use.
TEST(HistogramTest, BasicTest) {
  // Try basic construction
  Histogram* histogram(Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), histogram);

  Histogram* linear_histogram(LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), linear_histogram);

  vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  Histogram* custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, Histogram::kNoFlags));
  EXPECT_NE(reinterpret_cast<Histogram*>(NULL), custom_histogram);

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS("Test3Histogram", 30);

  DHISTOGRAM_TIMES("Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS("Test5Histogram", 30);

  HISTOGRAM_ENUMERATION("Test6Histogram", 129, 130);
}

TEST(HistogramTest, ExponentialRangesTest) {
  // Check that we got a nice exponential when there was enough rooom.
  BucketRanges ranges(9);
  Histogram::InitializeBucketRanges(1, 64, 8, &ranges);
  EXPECT_EQ(0, ranges.range(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, ranges.range(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges.range(8));

  // Check the corresponding Histogram will use the correct ranges.
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));
  EXPECT_TRUE(ranges.Equals(histogram->bucket_ranges()));

  // When bucket count is limited, exponential ranges will partially look like
  // linear.
  BucketRanges ranges2(16);
  Histogram::InitializeBucketRanges(1, 32, 15, &ranges2);

  EXPECT_EQ(0, ranges2.range(0));
  EXPECT_EQ(1, ranges2.range(1));
  EXPECT_EQ(2, ranges2.range(2));
  EXPECT_EQ(3, ranges2.range(3));
  EXPECT_EQ(4, ranges2.range(4));
  EXPECT_EQ(5, ranges2.range(5));
  EXPECT_EQ(6, ranges2.range(6));
  EXPECT_EQ(7, ranges2.range(7));
  EXPECT_EQ(9, ranges2.range(8));
  EXPECT_EQ(11, ranges2.range(9));
  EXPECT_EQ(14, ranges2.range(10));
  EXPECT_EQ(17, ranges2.range(11));
  EXPECT_EQ(21, ranges2.range(12));
  EXPECT_EQ(26, ranges2.range(13));
  EXPECT_EQ(32, ranges2.range(14));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges2.range(15));

  // Check the corresponding Histogram will use the correct ranges.
  Histogram* histogram2(Histogram::FactoryGet(
      "Histogram2", 1, 32, 15, Histogram::kNoFlags));
  EXPECT_TRUE(ranges2.Equals(histogram2->bucket_ranges()));
}

TEST(HistogramTest, LinearRangesTest) {
  BucketRanges ranges(9);
  LinearHistogram::InitializeBucketRanges(1, 7, 8, &ranges);
  // Gets a nice linear set of bucket ranges.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, ranges.range(i));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges.range(8));
  // The correspoding LinearHistogram should use the correct ranges.
  Histogram* histogram(
      LinearHistogram::FactoryGet("Linear", 1, 7, 8, Histogram::kNoFlags));
  EXPECT_TRUE(ranges.Equals(histogram->bucket_ranges()));

  // Linear ranges are not divisible.
  BucketRanges ranges2(6);
  LinearHistogram::InitializeBucketRanges(1, 6, 5, &ranges2);
  EXPECT_EQ(0, ranges2.range(0));
  EXPECT_EQ(1, ranges2.range(1));
  EXPECT_EQ(3, ranges2.range(2));
  EXPECT_EQ(4, ranges2.range(3));
  EXPECT_EQ(6, ranges2.range(4));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges2.range(5));
  // The correspoding LinearHistogram should use the correct ranges.
  Histogram* histogram2(
      LinearHistogram::FactoryGet("Linear2", 1, 6, 5, Histogram::kNoFlags));
  EXPECT_TRUE(ranges2.Equals(histogram2->bucket_ranges()));
}

TEST(HistogramTest, ArrayToCustomRangesTest) {
  const HistogramBase::Sample ranges[3] = {5, 10 ,20};
  vector<HistogramBase::Sample> ranges_vec =
      CustomHistogram::ArrayToCustomRanges(ranges, 3);
  ASSERT_EQ(6u, ranges_vec.size());
  EXPECT_EQ(5, ranges_vec[0]);
  EXPECT_EQ(6, ranges_vec[1]);
  EXPECT_EQ(10, ranges_vec[2]);
  EXPECT_EQ(11, ranges_vec[3]);
  EXPECT_EQ(20, ranges_vec[4]);
  EXPECT_EQ(21, ranges_vec[5]);
}

TEST(HistogramTest, CustomHistogramTest) {
  // A well prepared custom ranges.
  vector<HistogramBase::Sample> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(2);
  Histogram* histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram1", custom_ranges, Histogram::kNoFlags);
  const BucketRanges* ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));  // Auto added.
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(2, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));  // Auto added.

  // A unordered custom ranges.
  custom_ranges.clear();
  custom_ranges.push_back(2);
  custom_ranges.push_back(1);
  histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram2", custom_ranges, Histogram::kNoFlags);
  ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(2, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));

  // A custom ranges with duplicated values.
  custom_ranges.clear();
  custom_ranges.push_back(4);
  custom_ranges.push_back(1);
  custom_ranges.push_back(4);
  histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram3", custom_ranges, Histogram::kNoFlags);
  ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(4, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));
}

TEST(HistogramTest, CustomHistogramWithOnly2Buckets) {
  // This test exploits the fact that the CustomHistogram can have 2 buckets,
  // while the base class Histogram is *supposed* to have at least 3 buckets.
  // We should probably change the restriction on the base class (or not inherit
  // the base class!).

  vector<HistogramBase::Sample> custom_ranges;
  custom_ranges.push_back(4);

  Histogram* histogram = CustomHistogram::FactoryGet(
      "2BucketsCustomHistogram", custom_ranges, Histogram::kNoFlags);
  const BucketRanges* ranges = histogram->bucket_ranges();
  ASSERT_EQ(3u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(4, ranges->range(1));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(2));
}

// Make sure histogram handles out-of-bounds data gracefully.
TEST(HistogramTest, BoundsTest) {
  const size_t kBucketCount = 50;
  Histogram* histogram(Histogram::FactoryGet(
      "Bounded", 10, 100, kBucketCount, Histogram::kNoFlags));

  // Put two samples "out of bounds" above and below.
  histogram->Add(5);
  histogram->Add(-50);

  histogram->Add(100);
  histogram->Add(10000);

  // Verify they landed in the underflow, and overflow buckets.
  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  EXPECT_EQ(2, sample.counts(0));
  EXPECT_EQ(0, sample.counts(1));
  size_t array_size = histogram->bucket_count();
  EXPECT_EQ(kBucketCount, array_size);
  EXPECT_EQ(0, sample.counts(array_size - 2));
  EXPECT_EQ(2, sample.counts(array_size - 1));

  vector<int> custom_ranges;
  custom_ranges.push_back(10);
  custom_ranges.push_back(50);
  custom_ranges.push_back(100);
  Histogram* test_custom_histogram(CustomHistogram::FactoryGet(
      "TestCustomRangeBoundedHistogram", custom_ranges, Histogram::kNoFlags));

  // Put two samples "out of bounds" above and below.
  test_custom_histogram->Add(5);
  test_custom_histogram->Add(-50);
  test_custom_histogram->Add(100);
  test_custom_histogram->Add(1000);

  // Verify they landed in the underflow, and overflow buckets.
  Histogram::SampleSet custom_sample;
  test_custom_histogram->SnapshotSample(&custom_sample);
  EXPECT_EQ(2, custom_sample.counts(0));
  EXPECT_EQ(0, custom_sample.counts(1));
  size_t custom_array_size = test_custom_histogram->bucket_count();
  EXPECT_EQ(0, custom_sample.counts(custom_array_size - 2));
  EXPECT_EQ(2, custom_sample.counts(custom_array_size - 1));
}

// Check to be sure samples land as expected is "correct" buckets.
TEST(HistogramTest, BucketPlacementTest) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));

  // Add i+1 samples to the i'th bucket.
  histogram->Add(0);
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    for (int j = 0; j <= i; j++)
      histogram->Add(power_of_2);
    power_of_2 *= 2;
  }

  // Check to see that the bucket counts reflect our additions.
  Histogram::SampleSet sample;
  histogram->SnapshotSample(&sample);
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i + 1, sample.counts(i));
}

TEST(HistogramTest, CorruptSampleCounts) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.

  EXPECT_EQ(0, histogram->sample_.redundant_count());
  histogram->Add(20);  // Add some samples.
  histogram->Add(40);
  EXPECT_EQ(2, histogram->sample_.redundant_count());

  Histogram::SampleSet snapshot;
  histogram->SnapshotSample(&snapshot);
  EXPECT_EQ(Histogram::NO_INCONSISTENCIES, 0);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));  // No default corruption.
  EXPECT_EQ(2, snapshot.redundant_count());

  snapshot.counts_[3] += 100;  // Sample count won't match redundant count.
  EXPECT_EQ(Histogram::COUNT_LOW_ERROR, histogram->FindCorruption(snapshot));
  snapshot.counts_[2] -= 200;
  EXPECT_EQ(Histogram::COUNT_HIGH_ERROR, histogram->FindCorruption(snapshot));

  // But we can't spot a corruption if it is compensated for.
  snapshot.counts_[1] += 100;
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));
}

TEST(HistogramTest, CorruptBucketBounds) {
  Histogram* histogram(Histogram::FactoryGet(
      "Histogram", 1, 64, 8, Histogram::kNoFlags));  // As per header file.

  Histogram::SampleSet snapshot;
  histogram->SnapshotSample(&snapshot);
  EXPECT_EQ(Histogram::NO_INCONSISTENCIES, 0);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));  // No default corruption.

  BucketRanges* bucket_ranges =
      const_cast<BucketRanges*>(histogram->bucket_ranges());
  HistogramBase::Sample tmp = bucket_ranges->range(1);
  bucket_ranges->set_range(1, bucket_ranges->range(2));
  bucket_ranges->set_range(2, tmp);
  EXPECT_EQ(Histogram::BUCKET_ORDER_ERROR | Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  bucket_ranges->set_range(2, bucket_ranges->range(1));
  bucket_ranges->set_range(1, tmp);
  EXPECT_EQ(0, histogram->FindCorruption(snapshot));

  bucket_ranges->set_range(3, bucket_ranges->range(3) + 1);
  EXPECT_EQ(Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  // Show that two simple changes don't offset each other
  bucket_ranges->set_range(4, bucket_ranges->range(4) - 1);
  EXPECT_EQ(Histogram::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(snapshot));

  // Repair histogram so that destructor won't DCHECK().
  bucket_ranges->set_range(3, bucket_ranges->range(3) - 1);
  bucket_ranges->set_range(4, bucket_ranges->range(4) + 1);
}

#if GTEST_HAS_DEATH_TEST
// For Histogram, LinearHistogram and CustomHistogram, the minimum for a
// declared range is 1, while the maximum is (HistogramBase::kSampleType_MAX -
// 1). But we accept ranges exceeding those limits, and silently clamped to
// those limits. This is for backwards compatibility.
TEST(HistogramDeathTest, BadRangesTest) {
  Histogram* histogram = Histogram::FactoryGet(
      "BadRanges", 0, HistogramBase::kSampleType_MAX, 8, Histogram::kNoFlags);
  EXPECT_EQ(1, histogram->declared_min());
  EXPECT_EQ(HistogramBase::kSampleType_MAX - 1, histogram->declared_max());

  Histogram* linear_histogram = LinearHistogram::FactoryGet(
      "BadRangesLinear", 0, HistogramBase::kSampleType_MAX, 8,
      Histogram::kNoFlags);
  EXPECT_EQ(1, linear_histogram->declared_min());
  EXPECT_EQ(HistogramBase::kSampleType_MAX - 1,
            linear_histogram->declared_max());

  vector<int> custom_ranges;
  custom_ranges.push_back(0);
  custom_ranges.push_back(5);
  Histogram* custom_histogram1 = CustomHistogram::FactoryGet(
      "BadRangesCustom", custom_ranges, Histogram::kNoFlags);
  const BucketRanges* ranges = custom_histogram1->bucket_ranges();
  ASSERT_EQ(3u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(5, ranges->range(1));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(2));

  // CustomHistogram does not accepts kSampleType_MAX as range.
  custom_ranges.push_back(HistogramBase::kSampleType_MAX);
  EXPECT_DEATH(CustomHistogram::FactoryGet("BadRangesCustom2", custom_ranges,
                                           Histogram::kNoFlags),
               "");

  // CustomHistogram needs at least 1 valid range.
  custom_ranges.clear();
  custom_ranges.push_back(0);
  EXPECT_DEATH(CustomHistogram::FactoryGet("BadRangesCustom3", custom_ranges,
                                           Histogram::kNoFlags),
               "");
}
#endif

}  // namespace base
