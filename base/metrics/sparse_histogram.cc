// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include "base/metrics/statistics_recorder.h"

using std::string;

namespace base {

// static
HistogramBase* SparseHistogram::FactoryGet(const string& name,
                                           int32 flags) {
  // TODO(kaiwang): Register and get SparseHistogram with StatisticsRecorder.
  HistogramBase* histogram = new SparseHistogram(name);
  histogram->SetFlags(flags);
  return histogram;
}

SparseHistogram::~SparseHistogram() {}

void SparseHistogram::Add(Sample value) {
  base::AutoLock auto_lock(lock_);
  samples_[value]++;
}

void SparseHistogram::SnapshotSample(std::map<Sample, Count>* samples) const {
  base::AutoLock auto_lock(lock_);
  *samples = samples_;
}

void SparseHistogram::WriteHTMLGraph(string* output) const {
  // TODO(kaiwang): Implement.
}

void SparseHistogram::WriteAscii(string* output) const {
  // TODO(kaiwang): Implement.
}

SparseHistogram::SparseHistogram(const string& name)
    : HistogramBase(name) {}

void SparseHistogram::GetParameters(DictionaryValue* params) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

void SparseHistogram::GetCountAndBucketData(Count* count,
                                            ListValue* buckets) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

}  // namespace base
