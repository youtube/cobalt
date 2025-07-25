// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_CHURN_ACTIVE_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_CHURN_ACTIVE_STATUS_H_

#include <bitset>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

namespace system {
class StatisticsProvider;
}  // namespace system

namespace report::device_metrics {

// Manage information about the number of active months and the months since
// the device's inception, using specific bit sizes for each.
// Provide the methods for calculating churn analysis metadata.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) ActiveStatus {
 public:
  static constexpr int kMonthCountBitSize = 10;
  static constexpr int kActiveMonthsBitSize = 18;
  static constexpr int kActiveStatusBitSize =
      kMonthCountBitSize + kActiveMonthsBitSize;
  static constexpr int kInceptionYear = 2000;
  static constexpr int kNumberOfDaysInWeek = 7;
  static constexpr int kMonthsInYear = 12;

  // Calculate monthly and yearly churn for the 3 observation windows by
  // checking active status at bits 1-3 (monthly) and bits 13-15 (yearly).
  static constexpr int kMonthlyChurnOffset = 1;
  static constexpr int kYearlyChurnOffset = 13;

  // Track number of months from inception date in first 10 bits active status.
  static constexpr char kActiveStatusInceptionDate[] =
      "2000-01-01 00:00:00 GMT";

  // Default value for devices that are missing the activate date.
  static constexpr char kActivateDateKeyNotFound[] =
      "ACTIVATE_DATE_KEY_NOT_FOUND";

  ActiveStatus() = delete;
  explicit ActiveStatus(PrefService* local_state);
  ActiveStatus(const ActiveStatus&) = delete;
  ActiveStatus& operator=(const ActiveStatus&) = delete;
  ~ActiveStatus() = default;

  // Accessor methods for the active status value stored in local state pref.
  // This pref is used as the source of truth for this class.
  int GetValue() const;
  void SetValue(int val);

  // Returns a copy of the 28 bit updated active status value which reflects
  // the current month is also active.
  absl::optional<int> CalculateNewValue(base::Time ts) const;

  // The inception month and the month count since inception are utilized to
  // generate a fresh timestamp that signifies the presently active month.
  absl::optional<base::Time> GetCurrentActiveMonthTimestamp() const;

  // Return necessary information used to send the churn cohort request.
  absl::optional<ChurnCohortMetadata> CalculateCohortMetadata(
      base::Time active_ts) const;

  // Return necessary information used to send the churn observation request,
  // based on the local state last ping prefs.
  // |period| is a value between [0,2].
  // |period| = 0 indicates 3-month period starting current month.
  // |period| = 1 indicates 3-month period starting last month.
  // |period| = 2 indicates 3-month period starting two months ago.
  absl::optional<ChurnObservationMetadata> CalculateObservationMetadata(
      base::Time active_ts,
      int period) const;

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class ActiveStatusTest;

  // Return |kActiveStatusInceptionDate| as a GMT timestamp.
  absl::optional<base::Time> GetInceptionMonthTimestamp() const;

  // Return timestamp representing the first GMT monday of the year in |ts|.
  base::Time GetFirstMondayFromNewYear(base::Time ts) const;

  // The ActivateDate is formatted: YYYY-WW and is generated based on GMT date.
  // Return the first day of the ISO8601 week.
  absl::optional<base::Time> Iso8601DateWeekAsTime(
      int activate_year,
      int activate_week_of_year) const;

  // Get 1st day of the GMT based first active week, which uses ISO8601 date
  // (week) format. Field relies on ActivateDate VPD field, which is set
  // after OOBE is completed on the device for the first time.
  absl::optional<base::Time> GetFirstActiveWeek() const;

  // Return the int representation of the known months since inception, based
  // on the active status value in local state pref.
  int GetMonthsSinceInception() const;

  // Return the int representation of the known active month bits, based on the
  // active status value in local state pref.
  int GetActiveMonthBits() const;

  // Check if device was active during a specific month in the past 18 months.
  // |month_idx| is a value within the past 18 months, [0,17].
  bool IsDeviceActiveInMonth(int month_idx) const;

  // Helper to check if |active_ts| aligns with the first active month.
  absl::optional<bool> IsFirstActiveInCohort(base::Time active_ts) const;

  // Used to read/write the latest active status value.
  const raw_ptr<PrefService, ExperimentalAsh> local_state_;

  // Singleton lives throughout class lifetime.
  const raw_ptr<system::StatisticsProvider, ExperimentalAsh>
      statistics_provider_;
};

}  // namespace report::device_metrics
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_CHURN_ACTIVE_STATUS_H_
