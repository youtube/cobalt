// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_STATS_HISTOGRAM_H_
#define NET_DISK_CACHE_STATS_HISTOGRAM_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"

namespace base {
class BucketRanges;
class HistogramSamples;
class SampleVector;
}  // namespace base

namespace disk_cache {

class Stats;

// This class provides support for sending the disk cache size stats as a UMA
// histogram. We'll provide our own storage and management for the data, and a
// SampleVector with a copy of our data.
//
// Class derivation of Histogram "deprecated," and should not be copied, and
// may eventually go away.
//
class StatsHistogram : public base::Histogram {
 public:
  StatsHistogram(const std::string& name,
                 Sample minimum,
                 Sample maximum,
                 size_t bucket_count,
                 const base::BucketRanges* ranges,
                 const Stats* stats);
  virtual ~StatsHistogram();

  static void InitializeBucketRanges(const Stats* stats,
                                     base::BucketRanges* ranges);
  static StatsHistogram* FactoryGet(const std::string& name,
                                    const Stats* stats);

  virtual scoped_ptr<base::SampleVector> SnapshotSamples() const OVERRIDE;
  virtual Inconsistencies FindCorruption(
      const base::HistogramSamples& samples) const OVERRIDE;

 private:
  const Stats* stats_;
  DISALLOW_COPY_AND_ASSIGN(StatsHistogram);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_STATS_HISTOGRAM_H_
