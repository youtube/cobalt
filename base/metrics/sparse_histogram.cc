// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/lock.h"

using std::map;
using std::string;

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

// static
HistogramBase* SparseHistogram::FactoryGet(const string& name, int32 flags) {
  // TODO(kaiwang): Register and get SparseHistogram with StatisticsRecorder.
  HistogramBase* histogram = new SparseHistogram(name);
  histogram->SetFlags(flags);
  return histogram;
}

SparseHistogram::~SparseHistogram() {}

bool SparseHistogram::HasConstructionArguments(Sample minimum,
                                               Sample maximum,
                                               size_t bucket_count) const {
  // SparseHistogram never has min/max/bucket_count limit.
  return false;
}

void SparseHistogram::Add(Sample value) {
  base::AutoLock auto_lock(lock_);
  sample_counts_[value]++;
  redundant_count_ += 1;
}

scoped_ptr<HistogramSamples> SparseHistogram::SnapshotSamples() const {
  scoped_ptr<SampleMap> snapshot(new SampleMap());

  base::AutoLock auto_lock(lock_);
  for(map<Sample, Count>::const_iterator it = sample_counts_.begin();
      it != sample_counts_.end();
      ++it) {
    snapshot->Accumulate(it->first, it->second);
  }
  snapshot->ResetRedundantCount(redundant_count_);
  return snapshot.PassAs<HistogramSamples>();
}

void SparseHistogram::WriteHTMLGraph(string* output) const {
  // TODO(kaiwang): Implement.
}

void SparseHistogram::WriteAscii(string* output) const {
  // TODO(kaiwang): Implement.
}

SparseHistogram::SparseHistogram(const string& name)
    : HistogramBase(name),
      redundant_count_(0) {}

void SparseHistogram::GetParameters(DictionaryValue* params) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

void SparseHistogram::GetCountAndBucketData(Count* count,
                                            ListValue* buckets) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

}  // namespace base
