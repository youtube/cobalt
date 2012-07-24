// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatisticsRecorder handles all histograms in the system. It provides a
// general place for histograms to register, and supports a global API for
// accessing (i.e., dumping, or graphing) the data in all the histograms.

#ifndef BASE_METRICS_STATISTICS_RECORDER_H_
#define BASE_METRICS_STATISTICS_RECORDER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"

namespace base {

class BucketRanges;
class Histogram;
class Lock;

class BASE_EXPORT StatisticsRecorder {
 public:
  typedef std::vector<Histogram*> Histograms;

  // Initializes the StatisticsRecorder system.
  static void Initialize();

  // Find out if histograms can now be registered into our list.
  static bool IsActive();

  // Register, or add a new histogram to the collection of statistics. If an
  // identically named histogram is already registered, then the argument
  // |histogram| will deleted.  The returned value is always the registered
  // histogram (either the argument, or the pre-existing registered histogram).
  static Histogram* RegisterOrDeleteDuplicate(Histogram* histogram);

  // Register, or add a new bucket_ranges_ of |histogram|. If an identical
  // bucket_ranges_ is already registered, then the bucket_ranges_ of
  // |histogram| is deleted and the |histogram|'s bucket_ranges_ is reset to the
  // registered bucket_ranges_.  The bucket_ranges_ of |histogram| is always the
  // registered BucketRanges (either the argument's bucket_ranges_, or the
  // pre-existing registered bucket_ranges_).
  static void RegisterOrDeleteDuplicateRanges(Histogram* histogram);

  // Method for collecting stats about histograms created in browser and
  // renderer processes. |suffix| is appended to histogram names. |suffix| could
  // be either browser or renderer.
  static void CollectHistogramStats(const std::string& suffix);

  // Methods for printing histograms.  Only histograms which have query as
  // a substring are written to output (an empty string will process all
  // registered histograms).
  static void WriteHTMLGraph(const std::string& query, std::string* output);
  static void WriteGraph(const std::string& query, std::string* output);

  // Method for extracting histograms which were marked for use by UMA.
  static void GetHistograms(Histograms* output);

  // Find a histogram by name. It matches the exact name. This method is thread
  // safe.  It returns NULL if a matching histogram is not found.
  static Histogram* FindHistogram(const std::string& name);

  static bool dump_on_exit() { return dump_on_exit_; }

  static void set_dump_on_exit(bool enable) { dump_on_exit_ = enable; }

  // GetSnapshot copies some of the pointers to registered histograms into the
  // caller supplied vector (Histograms).  Only histograms with names matching
  // query are returned. The query must be a substring of histogram name for its
  // pointer to be copied.
  static void GetSnapshot(const std::string& query, Histograms* snapshot);

 private:
  // We keep all registered histograms in a map, from name to histogram.
  typedef std::map<std::string, Histogram*> HistogramMap;

  // We keep all |bucket_ranges_| in a map, from checksum to a list of
  // |bucket_ranges_|.  Checksum is calculated from the |ranges_| in
  // |bucket_ranges_|.
  typedef std::map<uint32, std::list<BucketRanges*>*> RangesMap;

  friend struct DefaultLazyInstanceTraits<StatisticsRecorder>;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, StartupShutdownTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, RecordedStartupTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, RangeTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CustomRangeTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, BucketRangesTest);

  StatisticsRecorder();

  ~StatisticsRecorder();

  static HistogramMap* histograms_;

  static RangesMap* ranges_;

  // lock protects access to the above map.
  static base::Lock* lock_;

  // Dump all known histograms to log.
  static bool dump_on_exit_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsRecorder);
};

}  // namespace base

#endif  // BASE_METRICS_STATISTICS_RECORDER_H_
