// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/base/ip_address.h"
#include "url/gurl.h"

namespace safe_browsing {

const int ClientSideDetectionServiceBase::kReportsIntervalDays = 1;
const int ClientSideDetectionServiceBase::kMaxReportsPerInterval = 3;
const int ClientSideDetectionServiceBase::kNegativeCacheIntervalDays = 1;
const int ClientSideDetectionServiceBase::kPositiveCacheIntervalMinutes = 30;

ClientSideDetectionServiceBase::CacheState::CacheState(bool phish,
                                                       base::Time time)
    : is_phishing(phish), timestamp(time) {}

ClientSideDetectionServiceBase::ClientSideDetectionServiceBase(
    PrefService* prefs)
    : prefs_(prefs) {}

ClientSideDetectionServiceBase::~ClientSideDetectionServiceBase() = default;

bool ClientSideDetectionServiceBase::IsPrivateIPAddress(
    const net::IPAddress& address) const {
  return !address.IsPubliclyRoutable();
}

void ClientSideDetectionServiceBase::UpdateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since we limit the number of requests but allow pass-through for cache
  // refreshes, we don't want to remove elements from the cache if they
  // could be used for this purpose even if we will not use the entry to
  // satisfy the request from the cache.
  base::TimeDelta positive_cache_interval =
      std::max(base::Minutes(kPositiveCacheIntervalMinutes),
               base::Days(kReportsIntervalDays));
  base::TimeDelta negative_cache_interval = std::max(
      base::Days(kNegativeCacheIntervalDays), base::Days(kReportsIntervalDays));

  // Remove elements from the cache that will no longer be used.
  for (auto it = cache_.begin(); it != cache_.end();) {
    const CacheState& cache_state = *it->second;
    if (cache_state.is_phishing
            ? cache_state.timestamp >
                  base::Time::Now() - positive_cache_interval
            : cache_state.timestamp >
                  base::Time::Now() - negative_cache_interval) {
      ++it;
    } else {
      cache_.erase(it++);
    }
  }
}

bool ClientSideDetectionServiceBase::GetValidCachedResult(const GURL& url,
                                                          bool* is_phishing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateCache();

  auto it = cache_.find(url);
  if (it == cache_.end()) {
    return false;
  }

  // We still need to check if the result is valid.
  const CacheState& cache_state = *it->second;
  if (cache_state.is_phishing
          ? cache_state.timestamp >
                base::Time::Now() - base::Minutes(kPositiveCacheIntervalMinutes)
          : cache_state.timestamp >
                base::Time::Now() - base::Days(kNegativeCacheIntervalDays)) {
    *is_phishing = cache_state.is_phishing;
    return true;
  }
  return false;
}

bool ClientSideDetectionServiceBase::AtPhishingReportLimit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clear the expired timestamps
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop_front();
  }

  // `delegate_` and prefs can be null in unit tests.
  if (base::FeatureList::IsEnabled(kSafeBrowsingDailyPhishingReportsLimit) &&
      prefs_ && IsEnhancedProtectionEnabled(*prefs_)) {
    return GetPhishingNumReports() >=
           kSafeBrowsingDailyPhishingReportsLimitESB.Get();
  }

  return GetPhishingNumReports() >= kMaxReportsPerInterval;
}

int ClientSideDetectionServiceBase::GetPhishingNumReports() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return phishing_report_times_.size();
}

bool ClientSideDetectionServiceBase::AddPhishingReport(base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We should not be adding a report when we are at the limit when this
  // function calls, but in case it does, we want to track how far back the
  // last report was prior to the current report and exit the function early.
  // Each classification request is made on the tab level, which may not have
  // had |phishing_report_times_| updated because the service class, that's on
  // the profile level, was processing a different request. Therefore, we check
  // one last time before we log the request.
  if (AtPhishingReportLimit()) {
    base::UmaHistogramMediumTimes("SBClientPhishing.TimeSinceLastReportAtLimit",
                                  timestamp - phishing_report_times_.back());
    return false;
  }

  if (!prefs_) {
    base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                              false);
    return false;
  }

  phishing_report_times_.push_back(timestamp);

  base::ListValue time_list;
  for (const base::Time& report_time : phishing_report_times_) {
    time_list.Append(base::Value(report_time.InSecondsFSinceUnixEpoch()));
  }
  prefs_->SetList(prefs::kSafeBrowsingCsdPingTimestamps, std::move(time_list));
  base::UmaHistogramBoolean("SBClientPhishing.AddPhishingReportSuccessful",
                            true);

  return true;
}

void ClientSideDetectionServiceBase::AddCacheEntry(const GURL& url,
                                                   bool is_phishing,
                                                   base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_[url] = std::make_unique<CacheState>(is_phishing, timestamp);
}

void ClientSideDetectionServiceBase::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_.clear();
}

void ClientSideDetectionServiceBase::LoadPhishingReportTimesFromPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // delegate and prefs can be null in unit tests.
  if (!prefs_) {
    return;
  }

  phishing_report_times_.clear();
  const auto cutoff = base::Time::Now() - base::Days(kReportsIntervalDays);
  for (const base::Value& timestamp :
       prefs_->GetList(prefs::kSafeBrowsingCsdPingTimestamps)) {
    auto time = base::Time::FromSecondsSinceUnixEpoch(timestamp.GetDouble());
    if (time >= cutoff) {
      phishing_report_times_.push_back(time);
    }
  }
}

}  // namespace safe_browsing
