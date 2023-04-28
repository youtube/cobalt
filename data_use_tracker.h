// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DATA_USE_TRACKER_H_
#define COMPONENTS_METRICS_DATA_USE_TRACKER_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

typedef base::Callback<void(const std::string&, int, bool)>
    UpdateUsagePrefCallbackType;

// Records the data use of user traffic and UMA traffic in user prefs. Taking
// into account those prefs it can verify whether certain UMA log upload is
// allowed.
class DataUseTracker {
 public:
  explicit DataUseTracker(PrefService* local_state);
  virtual ~DataUseTracker();

  // Returns an instance of |DataUseTracker| with provided |local_state| if
  // users data use should be tracked and null pointer otherwise.
  static std::unique_ptr<DataUseTracker> Create(PrefService* local_state);

  // Registers data use prefs using provided |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Updates data usage tracking prefs with the specified values.
  void UpdateMetricsUsagePrefs(const std::string& service_name,
                               int message_size,
                               bool is_cellular);

  // Returns whether a log with provided |log_bytes| can be uploaded according
  // to data use ratio and UMA quota provided by variations.
  bool ShouldUploadLogOnCellular(int log_bytes);

 private:
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckUpdateUsagePref);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckRemoveExpiredEntries);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckComputeTotalDataUse);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckCanUploadUMALog);

  // Updates provided |pref_name| for a current date with the given message
  // size.
  void UpdateUsagePref(const std::string& pref_name, int message_size);

  // Removes entries from the all data use  prefs.
  void RemoveExpiredEntries();

  // Removes entries from the given |pref_name| if they are more than 7 days
  // old.
  void RemoveExpiredEntriesForPref(const std::string& pref_name);

  // Computes data usage according to all the entries in the given dictionary
  // pref.
  int ComputeTotalDataUse(const std::string& pref_name);

  // Returns the weekly allowed quota for UMA data use.
  virtual bool GetUmaWeeklyQuota(int* uma_weekly_quota_bytes) const;

  // Returns the allowed ratio for UMA data use over overall data use.
  virtual bool GetUmaRatio(double* ratio) const;

  // Returns the current date for measurement.
  virtual base::Time GetCurrentMeasurementDate() const;

  // Returns the current date as a string with a proper formatting.
  virtual std::string GetCurrentMeasurementDateAsString() const;

  PrefService* local_state_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DataUseTracker);
};

}  // namespace metrics
#endif  // COMPONENTS_METRICS_DATA_USE_TRACKER_H_
