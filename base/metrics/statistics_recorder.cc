// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"

namespace base {

// Collect the number of histograms created.
static uint32 number_of_histograms_ = 0;
// Collect the number of vectors saved because of caching ranges.
static uint32 number_of_vectors_saved_ = 0;
// Collect the number of ranges_ elements saved because of caching ranges.
static size_t saved_ranges_size_ = 0;

// This singleton instance should be started during the single threaded portion
// of main(), and hence it is not thread safe.  It initializes globals to
// provide support for all future calls.
StatisticsRecorder::StatisticsRecorder() {
  DCHECK(!histograms_);
  if (lock_ == NULL) {
    // This will leak on purpose. It's the only way to make sure we won't race
    // against the static uninitialization of the module while one of our
    // static methods relying on the lock get called at an inappropriate time
    // during the termination phase. Since it's a static data member, we will
    // leak one per process, which would be similar to the instance allocated
    // during static initialization and released only on  process termination.
    lock_ = new base::Lock;
  }
  base::AutoLock auto_lock(*lock_);
  histograms_ = new HistogramMap;
  ranges_ = new RangesMap;
}

StatisticsRecorder::~StatisticsRecorder() {
  DCHECK(histograms_ && lock_);

  if (dump_on_exit_) {
    std::string output;
    WriteGraph("", &output);
    DLOG(INFO) << output;
  }
  // Clean up.
  HistogramMap* histograms = NULL;
  {
    base::AutoLock auto_lock(*lock_);
    histograms = histograms_;
    histograms_ = NULL;
  }
  RangesMap* ranges = NULL;
  {
    base::AutoLock auto_lock(*lock_);
    ranges = ranges_;
    ranges_ = NULL;
  }
  // We are going to leak the histograms and the ranges.
  delete histograms;
  delete ranges;
  // We don't delete lock_ on purpose to avoid having to properly protect
  // against it going away after we checked for NULL in the static methods.
}

// static
bool StatisticsRecorder::IsActive() {
  if (lock_ == NULL)
    return false;
  base::AutoLock auto_lock(*lock_);
  return NULL != histograms_;
}

Histogram* StatisticsRecorder::RegisterOrDeleteDuplicate(Histogram* histogram) {
  // As per crbug.com/79322 the histograms are intentionally leaked, so we need
  // to annotate them. Because ANNOTATE_LEAKING_OBJECT_PTR may be used only once
  // for an object, the duplicates should not be annotated.
  // Callers are responsible for not calling RegisterOrDeleteDuplicate(ptr)
  // twice if (lock_ == NULL) || (!histograms_).
  DCHECK(histogram->HasValidRangeChecksum());
  if (lock_ == NULL) {
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    return histogram;
  }
  base::AutoLock auto_lock(*lock_);
  if (!histograms_) {
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    return histogram;
  }
  const std::string name = histogram->histogram_name();
  HistogramMap::iterator it = histograms_->find(name);
  // Avoid overwriting a previous registration.
  if (histograms_->end() == it) {
    (*histograms_)[name] = histogram;
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    RegisterOrDeleteDuplicateRanges(histogram);
    ++number_of_histograms_;
  } else {
    delete histogram;  // We already have one by this name.
    histogram = it->second;
  }
  return histogram;
}

// static
void StatisticsRecorder::RegisterOrDeleteDuplicateRanges(Histogram* histogram) {
  DCHECK(histogram);
  CachedRanges* histogram_ranges = histogram->cached_ranges();
  DCHECK(histogram_ranges);
  uint32 checksum = histogram->range_checksum();
  histogram_ranges->SetRangeChecksum(checksum);

  RangesMap::iterator ranges_it = ranges_->find(checksum);
  if (ranges_->end() == ranges_it) {
    // Register the new CachedRanges.
    std::list<CachedRanges*>* checksum_matching_list(
        new std::list<CachedRanges*>());
    checksum_matching_list->push_front(histogram_ranges);
    (*ranges_)[checksum] = checksum_matching_list;
    return;
  }

  // Use the registered CachedRanges if the registered CachedRanges has same
  // ranges_ as |histogram|'s CachedRanges.
  std::list<CachedRanges*>* checksum_matching_list = ranges_it->second;
  std::list<CachedRanges*>::iterator checksum_matching_list_it;
  for (checksum_matching_list_it = checksum_matching_list->begin();
       checksum_matching_list_it != checksum_matching_list->end();
       ++checksum_matching_list_it) {
    CachedRanges* existing_histogram_ranges = *checksum_matching_list_it;
    DCHECK(existing_histogram_ranges);
    if (existing_histogram_ranges->Equals(histogram_ranges)) {
      histogram->set_cached_ranges(existing_histogram_ranges);
      ++number_of_vectors_saved_;
      saved_ranges_size_ += histogram_ranges->size();
      delete histogram_ranges;
      return;
    }
  }

  // We haven't found a CachedRanges which has the same ranges. Register the
  // new CachedRanges.
  DCHECK(checksum_matching_list_it == checksum_matching_list->end());
  checksum_matching_list->push_front(histogram_ranges);
}

// static
void StatisticsRecorder::CollectHistogramStats(const std::string& suffix) {
  static int uma_upload_attempt = 0;
  ++uma_upload_attempt;
  if (uma_upload_attempt == 1) {
    UMA_HISTOGRAM_COUNTS_10000(
        "Histogram.SharedRange.Count.FirstUpload." + suffix,
        number_of_histograms_);
    UMA_HISTOGRAM_COUNTS_10000(
        "Histogram.SharedRange.RangesSaved.FirstUpload." + suffix,
        number_of_vectors_saved_);
    UMA_HISTOGRAM_COUNTS(
        "Histogram.SharedRange.ElementsSaved.FirstUpload." + suffix,
        static_cast<int>(saved_ranges_size_));
    number_of_histograms_ = 0;
    number_of_vectors_saved_ = 0;
    saved_ranges_size_ = 0;
    return;
  }
  if (uma_upload_attempt == 2) {
    UMA_HISTOGRAM_COUNTS_10000(
        "Histogram.SharedRange.Count.SecondUpload." + suffix,
        number_of_histograms_);
    UMA_HISTOGRAM_COUNTS_10000(
        "Histogram.SharedRange.RangesSaved.SecondUpload." + suffix,
        number_of_vectors_saved_);
    UMA_HISTOGRAM_COUNTS(
        "Histogram.SharedRange.ElementsSaved.SecondUpload." + suffix,
        static_cast<int>(saved_ranges_size_));
    number_of_histograms_ = 0;
    number_of_vectors_saved_ = 0;
    saved_ranges_size_ = 0;
    return;
  }
  UMA_HISTOGRAM_COUNTS_10000(
      "Histogram.SharedRange.Count.RestOfUploads." + suffix,
      number_of_histograms_);
  UMA_HISTOGRAM_COUNTS_10000(
      "Histogram.SharedRange.RangesSaved.RestOfUploads." + suffix,
      number_of_vectors_saved_);
  UMA_HISTOGRAM_COUNTS(
      "Histogram.SharedRange.ElementsSaved.RestOfUploads." + suffix,
      static_cast<int>(saved_ranges_size_));
}

// static
void StatisticsRecorder::WriteHTMLGraph(const std::string& query,
                                        std::string* output) {
  if (!IsActive())
    return;

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteHTMLGraph(output);
    output->append("<br><hr><br>");
  }
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (!IsActive())
    return;
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  for (Histograms::iterator it = snapshot.begin();
       it != snapshot.end();
       ++it) {
    (*it)->WriteAscii(output);
    output->append("\n");
  }
}

// static
void StatisticsRecorder::GetHistograms(Histograms* output) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    DCHECK_EQ(it->first, it->second->histogram_name());
    output->push_back(it->second);
  }
}

// static
Histogram* StatisticsRecorder::FindHistogram(const std::string& name) {
  if (lock_ == NULL)
    return NULL;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return NULL;
  HistogramMap::iterator it = histograms_->find(name);
  if (histograms_->end() == it)
    return NULL;
  return it->second;
}

// private static
void StatisticsRecorder::GetSnapshot(const std::string& query,
                                     Histograms* snapshot) {
  if (lock_ == NULL)
    return;
  base::AutoLock auto_lock(*lock_);
  if (!histograms_)
    return;
  for (HistogramMap::iterator it = histograms_->begin();
       histograms_->end() != it;
       ++it) {
    if (it->first.find(query) != std::string::npos)
      snapshot->push_back(it->second);
  }
}

// static
StatisticsRecorder::HistogramMap* StatisticsRecorder::histograms_ = NULL;
// static
StatisticsRecorder::RangesMap* StatisticsRecorder::ranges_ = NULL;
// static
base::Lock* StatisticsRecorder::lock_ = NULL;
// static
bool StatisticsRecorder::dump_on_exit_ = false;

}  // namespace base
