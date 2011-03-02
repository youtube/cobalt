// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/stats_histogram.h"

#include "base/logging.h"
#include "net/disk_cache/stats.h"

namespace disk_cache {

using base::Histogram;
using base::StatisticsRecorder;

// Static.
const Stats* StatsHistogram::stats_ = NULL;

StatsHistogram::~StatsHistogram() {
  // Only cleanup what we set.
  if (init_)
    stats_ = NULL;
}

scoped_refptr<StatsHistogram> StatsHistogram::StatsHistogramFactoryGet(
    const std::string& name) {
  scoped_refptr<Histogram> histogram(NULL);

  Sample minimum = 1;
  Sample maximum = disk_cache::Stats::kDataSizesLength - 1;
  size_t bucket_count = disk_cache::Stats::kDataSizesLength;

  if (StatisticsRecorder::FindHistogram(name, &histogram)) {
    DCHECK(histogram.get() != NULL);
  } else {
    StatsHistogram* stats_histogram = new StatsHistogram(name, minimum, maximum,
                                                         bucket_count);
    stats_histogram->InitializeBucketRange();
    histogram = stats_histogram;
    StatisticsRecorder::Register(&histogram);
  }

  DCHECK(HISTOGRAM == histogram->histogram_type());
  DCHECK(histogram->HasConstructorArguments(minimum, maximum, bucket_count));

  // We're preparing for an otherwise unsafe upcast by ensuring we have the
  // proper class type.
  Histogram* temp_histogram = histogram.get();
  StatsHistogram* temp_stats_histogram =
      static_cast<StatsHistogram*>(temp_histogram);
  // Validate upcast by seeing that we're probably providing the checksum.
  CHECK_EQ(temp_stats_histogram->StatsHistogram::CalculateRangeChecksum(),
           temp_stats_histogram->CalculateRangeChecksum());
  scoped_refptr<StatsHistogram> return_histogram(temp_stats_histogram);
  return return_histogram;
}

bool StatsHistogram::Init(const Stats* stats) {
  DCHECK(stats);
  if (stats_)
    return false;

  SetFlags(kUmaTargetedHistogramFlag);

  // We support statistics report for only one cache.
  init_ = true;
  stats_ = stats;
  return true;
}

Histogram::Sample StatsHistogram::ranges(size_t i) const {
  DCHECK(stats_);
  return stats_->GetBucketRange(i);
}

size_t StatsHistogram::bucket_count() const {
  return disk_cache::Stats::kDataSizesLength;
}

void StatsHistogram::SnapshotSample(SampleSet* sample) const {
  DCHECK(stats_);
  StatsSamples my_sample;
  stats_->Snapshot(&my_sample);

  *sample = my_sample;

  // Only report UMA data once.
  StatsHistogram* mutable_me = const_cast<StatsHistogram*>(this);
  mutable_me->ClearFlags(kUmaTargetedHistogramFlag);
}

Histogram::Inconsistencies StatsHistogram::FindCorruption(
    const SampleSet& snapshot) const {
  return NO_INCONSISTENCIES;  // This class won't monitor inconsistencies.
}

uint32 StatsHistogram::CalculateRangeChecksum() const {
  // We don't calculate checksums, so at least establish a unique constant.
  const uint32 kStatsHistogramChecksum = 0x0cecce;
  return kStatsHistogramChecksum;
}

}  // namespace disk_cache
