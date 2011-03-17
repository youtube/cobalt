// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_entry.h"

#include <cmath>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "net/url_request/url_request_throttler_header_interface.h"

namespace net {

const int URLRequestThrottlerEntry::kDefaultSlidingWindowPeriodMs = 2000;
const int URLRequestThrottlerEntry::kDefaultMaxSendThreshold = 20;
const int URLRequestThrottlerEntry::kDefaultInitialBackoffMs = 700;
const double URLRequestThrottlerEntry::kDefaultMultiplyFactor = 1.4;
const double URLRequestThrottlerEntry::kDefaultJitterFactor = 0.4;
const int URLRequestThrottlerEntry::kDefaultMaximumBackoffMs = 60 * 60 * 1000;
const int URLRequestThrottlerEntry::kDefaultEntryLifetimeMs = 120000;
const char URLRequestThrottlerEntry::kRetryHeaderName[] = "X-Retry-After";

URLRequestThrottlerEntry::URLRequestThrottlerEntry()
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(kDefaultSlidingWindowPeriodMs)),
      max_send_threshold_(kDefaultMaxSendThreshold),
      backoff_entry_(&backoff_policy_) {
  Initialize();
}

URLRequestThrottlerEntry::URLRequestThrottlerEntry(
    int sliding_window_period_ms,
    int max_send_threshold,
    int initial_backoff_ms,
    double multiply_factor,
    double jitter_factor,
    int maximum_backoff_ms)
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(sliding_window_period_ms)),
      max_send_threshold_(max_send_threshold),
      backoff_entry_(&backoff_policy_) {
  DCHECK_GT(sliding_window_period_ms, 0);
  DCHECK_GT(max_send_threshold_, 0);
  DCHECK_GE(initial_backoff_ms, 0);
  DCHECK_GT(multiply_factor, 0);
  DCHECK_GE(jitter_factor, 0.0);
  DCHECK_LT(jitter_factor, 1.0);
  DCHECK_GE(maximum_backoff_ms, 0);

  Initialize();
  backoff_policy_.initial_backoff_ms = initial_backoff_ms;
  backoff_policy_.multiply_factor = multiply_factor;
  backoff_policy_.jitter_factor = jitter_factor;
  backoff_policy_.maximum_backoff_ms = maximum_backoff_ms;
  backoff_policy_.entry_lifetime_ms = -1;
}

bool URLRequestThrottlerEntry::IsEntryOutdated() const {
  // If there are send events in the sliding window period, we still need this
  // entry.
  if (!send_log_.empty() &&
      send_log_.back() + sliding_window_period_ > GetTimeNow()) {
    return false;
  }

  return GetBackoffEntry()->CanDiscard();
}

bool URLRequestThrottlerEntry::IsDuringExponentialBackoff() const {
  return GetBackoffEntry()->ShouldRejectRequest();
}

int64 URLRequestThrottlerEntry::ReserveSendingTimeForNextRequest(
    const base::TimeTicks& earliest_time) {
  base::TimeTicks now = GetTimeNow();

  // If a lot of requests were successfully made recently,
  // sliding_window_release_time_ may be greater than
  // exponential_backoff_release_time_.
  base::TimeTicks recommended_sending_time =
      std::max(std::max(now, earliest_time),
               std::max(GetBackoffEntry()->GetReleaseTime(),
                        sliding_window_release_time_));

  DCHECK(send_log_.empty() ||
         recommended_sending_time >= send_log_.back());
  // Log the new send event.
  send_log_.push(recommended_sending_time);

  sliding_window_release_time_ = recommended_sending_time;

  // Drop the out-of-date events in the event list.
  // We don't need to worry that the queue may become empty during this
  // operation, since the last element is sliding_window_release_time_.
  while ((send_log_.front() + sliding_window_period_ <=
          sliding_window_release_time_) ||
         send_log_.size() > static_cast<unsigned>(max_send_threshold_)) {
    send_log_.pop();
  }

  // Check if there are too many send events in recent time.
  if (send_log_.size() == static_cast<unsigned>(max_send_threshold_))
    sliding_window_release_time_ = send_log_.front() + sliding_window_period_;

  return (recommended_sending_time - now).InMillisecondsRoundedUp();
}

base::TimeTicks
    URLRequestThrottlerEntry::GetExponentialBackoffReleaseTime() const {
  return GetBackoffEntry()->GetReleaseTime();
}

void URLRequestThrottlerEntry::UpdateWithResponse(
    const URLRequestThrottlerHeaderInterface* response) {
  if (response->GetResponseCode() >= 500) {
    GetBackoffEntry()->InformOfRequest(false);
  } else {
    GetBackoffEntry()->InformOfRequest(true);

    std::string retry_header = response->GetNormalizedValue(kRetryHeaderName);
    if (!retry_header.empty())
      HandleCustomRetryAfter(retry_header);
  }
}

void URLRequestThrottlerEntry::ReceivedContentWasMalformed() {
  // We keep this simple and just count it as a single error.
  //
  // If we wanted to get fancy, we would count two errors here, and decrease
  // the error count only by one when we receive a successful (by status
  // code) response. Instead, we keep things simple by always resetting the
  // error count on success, and therefore counting only a single error here.
  GetBackoffEntry()->InformOfRequest(false);
}

URLRequestThrottlerEntry::~URLRequestThrottlerEntry() {
}

void URLRequestThrottlerEntry::Initialize() {
  sliding_window_release_time_ = base::TimeTicks::Now();
  backoff_policy_.num_errors_to_ignore = 0;
  backoff_policy_.initial_backoff_ms = kDefaultInitialBackoffMs;
  backoff_policy_.multiply_factor = kDefaultMultiplyFactor;
  backoff_policy_.jitter_factor = kDefaultJitterFactor;
  backoff_policy_.maximum_backoff_ms = kDefaultMaximumBackoffMs;
  backoff_policy_.entry_lifetime_ms = kDefaultEntryLifetimeMs;
}

base::TimeTicks URLRequestThrottlerEntry::GetTimeNow() const {
  return base::TimeTicks::Now();
}

void URLRequestThrottlerEntry::HandleCustomRetryAfter(
    const std::string& header_value) {
  // Input parameter is the number of seconds to wait in a floating point value.
  double time_in_sec = 0;
  bool conversion_is_ok = base::StringToDouble(header_value, &time_in_sec);

  // Conversion of custom retry-after header value failed.
  if (!conversion_is_ok)
    return;

  // We must use an int value later so we transform this in milliseconds.
  int64 value_ms = static_cast<int64>(0.5 + time_in_sec * 1000);

  // We do not check for an upper bound; the server can set any Retry-After it
  // desires. Recovery from error would involve restarting the browser.
  if (value_ms < 0)
    return;

  GetBackoffEntry()->SetCustomReleaseTime(
      GetTimeNow() + base::TimeDelta::FromMilliseconds(value_ms));
}

const BackoffEntry* URLRequestThrottlerEntry::GetBackoffEntry() const {
  return &backoff_entry_;
}

BackoffEntry* URLRequestThrottlerEntry::GetBackoffEntry() {
  return &backoff_entry_;
}

}  // namespace net
