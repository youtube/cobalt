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
const int URLRequestThrottlerEntry::kDefaultAdditionalConstantMs = 100;
const double URLRequestThrottlerEntry::kDefaultMultiplyFactor = 1.4;
const double URLRequestThrottlerEntry::kDefaultJitterFactor = 0.4;
const int URLRequestThrottlerEntry::kDefaultMaximumBackoffMs = 60 * 60 * 1000;
const int URLRequestThrottlerEntry::kDefaultEntryLifetimeMs = 120000;
const char URLRequestThrottlerEntry::kRetryHeaderName[] = "X-Retry-After";

URLRequestThrottlerEntry::URLRequestThrottlerEntry()
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(kDefaultSlidingWindowPeriodMs)),
      max_send_threshold_(kDefaultMaxSendThreshold),
      initial_backoff_ms_(kDefaultInitialBackoffMs),
      additional_constant_ms_(kDefaultAdditionalConstantMs),
      multiply_factor_(kDefaultMultiplyFactor),
      jitter_factor_(kDefaultJitterFactor),
      maximum_backoff_ms_(kDefaultMaximumBackoffMs),
      entry_lifetime_ms_(kDefaultEntryLifetimeMs) {
  Initialize();
}

URLRequestThrottlerEntry::URLRequestThrottlerEntry(
    int sliding_window_period_ms,
    int max_send_threshold,
    int initial_backoff_ms,
    int additional_constant_ms,
    double multiply_factor,
    double jitter_factor,
    int maximum_backoff_ms)
    : sliding_window_period_(
          base::TimeDelta::FromMilliseconds(sliding_window_period_ms)),
      max_send_threshold_(max_send_threshold),
      initial_backoff_ms_(initial_backoff_ms),
      additional_constant_ms_(additional_constant_ms),
      multiply_factor_(multiply_factor),
      jitter_factor_(jitter_factor),
      maximum_backoff_ms_(maximum_backoff_ms),
      entry_lifetime_ms_(-1) {
  DCHECK_GT(sliding_window_period_ms, 0);
  DCHECK_GT(max_send_threshold_, 0);
  DCHECK_GE(initial_backoff_ms_, 0);
  DCHECK_GE(additional_constant_ms_, 0);
  DCHECK_GT(multiply_factor_, 0);
  DCHECK_GE(jitter_factor_, 0);
  DCHECK_LT(jitter_factor_, 1);
  DCHECK_GE(maximum_backoff_ms_, 0);

  Initialize();
}

bool URLRequestThrottlerEntry::IsEntryOutdated() const {
  CHECK(this);  // to help track crbug.com/71721
  if (entry_lifetime_ms_ == -1)
    return false;

  base::TimeTicks now = GetTimeNow();

  // If there are send events in the sliding window period, we still need this
  // entry.
  if (!send_log_.empty() &&
      send_log_.back() + sliding_window_period_ > now) {
    return false;
  }

  int64 unused_since_ms =
      (now - exponential_backoff_release_time_).InMilliseconds();

  // Release time is further than now, we are managing it.
  if (unused_since_ms < 0)
    return false;

  // latest_response_was_failure_ is true indicates that the latest one or
  // more requests encountered server errors or had malformed response bodies.
  // In that case, we don't want to collect the entry unless it hasn't been used
  // for longer than the maximum allowed back-off.
  if (latest_response_was_failure_)
    return unused_since_ms > std::max(maximum_backoff_ms_, entry_lifetime_ms_);

  // Otherwise, consider the entry is outdated if it hasn't been used for the
  // specified lifetime period.
  return unused_since_ms > entry_lifetime_ms_;
}

bool URLRequestThrottlerEntry::IsDuringExponentialBackoff() const {
  return exponential_backoff_release_time_ > GetTimeNow();
}

int64 URLRequestThrottlerEntry::ReserveSendingTimeForNextRequest(
    const base::TimeTicks& earliest_time) {
  base::TimeTicks now = GetTimeNow();
  // If a lot of requests were successfully made recently,
  // sliding_window_release_time_ may be greater than
  // exponential_backoff_release_time_.
  base::TimeTicks recommended_sending_time =
      std::max(std::max(now, earliest_time),
               std::max(exponential_backoff_release_time_,
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
  return exponential_backoff_release_time_;
}

void URLRequestThrottlerEntry::UpdateWithResponse(
    const URLRequestThrottlerHeaderInterface* response) {
  if (response->GetResponseCode() >= 500) {
    failure_count_++;
    latest_response_was_failure_ = true;
    exponential_backoff_release_time_ =
        CalculateExponentialBackoffReleaseTime();
  } else {
    // We slowly decay the number of times delayed instead of resetting it to 0
    // in order to stay stable if we received lots of requests with
    // malformed bodies at the same time.
    if (failure_count_ > 0)
      failure_count_--;

    latest_response_was_failure_ = false;

    // The reason why we are not just cutting the release time to GetTimeNow()
    // is on the one hand, it would unset delay put by our custom retry-after
    // header and on the other we would like to push every request up to our
    // "horizon" when dealing with multiple in-flight requests. Ex: If we send
    // three requests and we receive 2 failures and 1 success. The success that
    // follows those failures will not reset the release time, further requests
    // will then need to wait the delay caused by the 2 failures.
    exponential_backoff_release_time_ = std::max(
        GetTimeNow(), exponential_backoff_release_time_);

    std::string retry_header = response->GetNormalizedValue(kRetryHeaderName);
    if (!retry_header.empty())
      HandleCustomRetryAfter(retry_header);
  }
}

void URLRequestThrottlerEntry::ReceivedContentWasMalformed() {
  // For any response that is marked as malformed now, we have probably
  // considered it as a success when receiving it and decreased the failure
  // count by 1. As a result, we increase the failure count by 2 here to undo
  // the effect and record a failure.
  //
  // Please note that this may lead to a larger failure count than expected,
  // because we don't decrease the failure count for successful responses when
  // it has already reached 0.
  failure_count_ += 2;
  latest_response_was_failure_ = true;
  exponential_backoff_release_time_ = CalculateExponentialBackoffReleaseTime();
}

void URLRequestThrottlerEntry::SetEntryLifetimeMsForTest(int lifetime_ms) {
  entry_lifetime_ms_ = lifetime_ms;
}

URLRequestThrottlerEntry::~URLRequestThrottlerEntry() {
}

void URLRequestThrottlerEntry::Initialize() {
  // Since this method is called by the constructors, GetTimeNow() (a virtual
  // method) is not used.
  exponential_backoff_release_time_ = base::TimeTicks::Now();
  failure_count_ = 0;
  latest_response_was_failure_ = false;

  sliding_window_release_time_ = base::TimeTicks::Now();
}

base::TimeTicks
    URLRequestThrottlerEntry::CalculateExponentialBackoffReleaseTime() {
  double delay = initial_backoff_ms_;
  delay *= pow(multiply_factor_, failure_count_);
  delay += additional_constant_ms_;
  delay -= base::RandDouble() * jitter_factor_ * delay;

  // Ensure that we do not exceed maximum delay.
  int64 delay_int = static_cast<int64>(delay + 0.5);
  delay_int = std::min(delay_int, static_cast<int64>(maximum_backoff_ms_));

  return std::max(GetTimeNow() + base::TimeDelta::FromMilliseconds(delay_int),
                  exponential_backoff_release_time_);
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

  if (maximum_backoff_ms_ < value_ms || value_ms < 0)
    return;

  exponential_backoff_release_time_ = std::max(
      (GetTimeNow() + base::TimeDelta::FromMilliseconds(value_ms)),
      exponential_backoff_release_time_);
}

}  // namespace net
