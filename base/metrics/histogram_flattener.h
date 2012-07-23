// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_FLATTENER_H_
#define BASE_METRICS_HISTOGRAM_FLATTENER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"

namespace base {

// HistogramFlattener is an interface for the logistics of gathering up
// available histograms for recording. The implementors handle the exact lower
// level recording mechanism, or error report mechanism.
class BASE_EXPORT HistogramFlattener {
 public:
  virtual void RecordDelta(const base::Histogram& histogram,
                           const base::Histogram::SampleSet& snapshot) = 0;

  // Record various errors found during attempts to record histograms.
  virtual void InconsistencyDetected(int problem) = 0;
  virtual void UniqueInconsistencyDetected(int problem) = 0;
  virtual void SnapshotProblemResolved(int amount) = 0;

 protected:
  HistogramFlattener() {}
  virtual ~HistogramFlattener() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HistogramFlattener);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_FLATTENER_H_
