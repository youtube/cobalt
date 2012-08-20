// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include "base/metrics/statistics_recorder.h"

using base::Histogram;
using base::StatisticsRecorder;

namespace base {

HistogramSnapshotManager::HistogramSnapshotManager(
    HistogramFlattener* histogram_flattener)
    : histogram_flattener_(histogram_flattener) {
  DCHECK(histogram_flattener_);
}

HistogramSnapshotManager::~HistogramSnapshotManager() {}

void HistogramSnapshotManager::PrepareDeltas(Histogram::Flags flag_to_set,
                                             bool record_only_uma) {
  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetHistograms(&histograms);
  for (StatisticsRecorder::Histograms::const_iterator it = histograms.begin();
       histograms.end() != it;
       ++it) {
    (*it)->SetFlags(flag_to_set);
    if (record_only_uma &&
        0 == ((*it)->flags() & Histogram::kUmaTargetedHistogramFlag))
      continue;
    PrepareDelta(**it);
  }
}

void HistogramSnapshotManager::PrepareDelta(const Histogram& histogram) {
  DCHECK(histogram_flattener_);

  // Get up-to-date snapshot of sample stats.
  Histogram::SampleSet snapshot;
  histogram.SnapshotSample(&snapshot);
  const std::string& histogram_name = histogram.histogram_name();

  int corruption = histogram.FindCorruption(snapshot);

  // Crash if we detect that our histograms have been overwritten.  This may be
  // a fair distance from the memory smasher, but we hope to correlate these
  // crashes with other events, such as plugins, or usage patterns, etc.
  if (Histogram::BUCKET_ORDER_ERROR & corruption) {
    // The checksum should have caught this, so crash separately if it didn't.
    CHECK_NE(0, Histogram::RANGE_CHECKSUM_ERROR & corruption);
    CHECK(false);  // Crash for the bucket order corruption.
  }
  // Checksum corruption might not have caused order corruption.
  CHECK_EQ(0, Histogram::RANGE_CHECKSUM_ERROR & corruption);

  // Note, at this point corruption can only be COUNT_HIGH_ERROR or
  // COUNT_LOW_ERROR and they never arise together, so we don't need to extract
  // bits from corruption.
  if (corruption) {
    NOTREACHED();
    histogram_flattener_->InconsistencyDetected(
        static_cast<Histogram::Inconsistencies>(corruption));
    // Don't record corrupt data to metrics survices.
    if (NULL == inconsistencies_.get())
      inconsistencies_.reset(new ProblemMap);
    int old_corruption = (*inconsistencies_)[histogram_name];
    if (old_corruption == (corruption | old_corruption))
      return;  // We've already seen this corruption for this histogram.
    (*inconsistencies_)[histogram_name] |= corruption;
    histogram_flattener_->UniqueInconsistencyDetected(
        static_cast<Histogram::Inconsistencies>(corruption));
    return;
  }

  // Find the already recorded stats, or create an empty set. Remove from our
  // snapshot anything that we've already recorded.
  LoggedSampleMap::iterator it = logged_samples_.find(histogram_name);
  Histogram::SampleSet* already_logged;
  if (logged_samples_.end() == it) {
    // Add new entry
    already_logged = &logged_samples_[histogram.histogram_name()];
    // Complete initialization.
    already_logged->Resize(histogram.bucket_count());
  } else {
    already_logged = &(it->second);
    int64 discrepancy(already_logged->TotalCount() -
                      already_logged->redundant_count());
    if (discrepancy) {
      NOTREACHED();  // Already_logged has become corrupt.
      int problem = static_cast<int>(discrepancy);
      if (problem != discrepancy)
        problem = INT_MAX;
      histogram_flattener_->InconsistencyDetectedInLoggedCount(problem);
      // With no valid baseline, we'll act like we've recorded everything in our
      // snapshot.
      already_logged->Subtract(*already_logged);
      already_logged->Add(snapshot);
    }
    // Deduct any stats we've already logged from our snapshot.
    snapshot.Subtract(*already_logged);
  }

  // Snapshot now contains only a delta to what we've already_logged.
  if (snapshot.redundant_count() > 0) {
    histogram_flattener_->RecordDelta(histogram, snapshot);
    // Add new data into our running total.
    already_logged->Add(snapshot);
  }
}
}  // namespace base
