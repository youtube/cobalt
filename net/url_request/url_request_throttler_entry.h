// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

// URLRequestThrottlerEntry represents an entry of URLRequestThrottlerManager.
// It analyzes requests of a specific URL over some period of time, in order to
// deduce the back-off time for every request.
// The back-off algorithm consists of two parts. Firstly, exponential back-off
// is used when receiving 5XX server errors or malformed response bodies.
// The exponential back-off rule is enforced by URLRequestHttpJob. Any
// request sent during the back-off period will be cancelled.
// Secondly, a sliding window is used to count recent requests to a given
// destination and provide guidance (to the application level only) on whether
// too many requests have been sent and when a good time to send the next one
// would be. This is never used to deny requests at the network level.
class URLRequestThrottlerEntry : public URLRequestThrottlerEntryInterface {
 public:
  // Sliding window period.
  static const int kDefaultSlidingWindowPeriodMs;

  // Maximum number of requests allowed in sliding window period.
  static const int kDefaultMaxSendThreshold;

  // Initial delay for exponential back-off.
  static const int kDefaultInitialBackoffMs;

  // Additional constant to adjust back-off.
  static const int kDefaultAdditionalConstantMs;

  // Factor by which the waiting time will be multiplied.
  static const double kDefaultMultiplyFactor;

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  static const double kDefaultJitterFactor;

  // Maximum amount of time we are willing to delay our request.
  static const int kDefaultMaximumBackoffMs;

  // Time after which the entry is considered outdated.
  static const int kDefaultEntryLifetimeMs;

  // Name of the header that servers can use to ask clients to delay their next
  // request.
  static const char kRetryHeaderName[];

  URLRequestThrottlerEntry();

  // The life span of instances created with this constructor is set to
  // infinite.
  // It is only used by unit tests.
  URLRequestThrottlerEntry(int sliding_window_period_ms,
                           int max_send_threshold,
                           int initial_backoff_ms,
                           int additional_constant_ms,
                           double multiply_factor,
                           double jitter_factor,
                           int maximum_backoff_ms);

  // Used by the manager, returns true if the entry needs to be garbage
  // collected.
  bool IsEntryOutdated() const;

  // Implementation of URLRequestThrottlerEntryInterface.
  virtual bool IsDuringExponentialBackoff() const;
  virtual int64 ReserveSendingTimeForNextRequest(
      const base::TimeTicks& earliest_time);
  virtual base::TimeTicks GetExponentialBackoffReleaseTime() const;
  virtual void UpdateWithResponse(
      const URLRequestThrottlerHeaderInterface* response);
  virtual void ReceivedContentWasMalformed();
  virtual void SetEntryLifetimeMsForTest(int lifetime_ms);

 protected:
  virtual ~URLRequestThrottlerEntry();

  void Initialize();

  // Calculates the release time for exponential back-off.
  base::TimeTicks CalculateExponentialBackoffReleaseTime();

  // Equivalent to TimeTicks::Now(), virtual to be mockable for testing purpose.
  virtual base::TimeTicks GetTimeNow() const;

  // Used internally to increase release time following a retry-after header.
  void HandleCustomRetryAfter(const std::string& header_value);

  // Used by tests.
  void set_exponential_backoff_release_time(
      const base::TimeTicks& release_time) {
    exponential_backoff_release_time_ = release_time;
  }

  // Used by tests.
  base::TimeTicks sliding_window_release_time() const {
    return sliding_window_release_time_;
  }

  // Used by tests.
  void set_sliding_window_release_time(const base::TimeTicks& release_time) {
    sliding_window_release_time_ = release_time;
  }

  // Used by tests.
  void set_failure_count(int failure_count) {
    failure_count_ = failure_count;
  }

 private:
  // Timestamp calculated by the exponential back-off algorithm at which we are
  // allowed to start sending requests again.
  base::TimeTicks exponential_backoff_release_time_;

  // Number of times we encounter server errors or malformed response bodies.
  int failure_count_;

  // If true, the last request response was a failure.
  // Note that this member can be false at the same time as failure_count_ can
  // be greater than 0, since we gradually decrease failure_count_, instead of
  // resetting it to 0 directly, when we receive successful responses.
  bool latest_response_was_failure_;

  // Timestamp calculated by the sliding window algorithm for when we advise
  // clients the next request should be made, at the earliest. Advisory only,
  // not used to deny requests.
  base::TimeTicks sliding_window_release_time_;

  // A list of the recent send events. We use them to decide whether there are
  // too many requests sent in sliding window.
  std::queue<base::TimeTicks> send_log_;

  const base::TimeDelta sliding_window_period_;
  const int max_send_threshold_;
  const int initial_backoff_ms_;
  const int additional_constant_ms_;
  const double multiply_factor_;
  const double jitter_factor_;
  const int maximum_backoff_ms_;
  // Set to -1 if the entry never expires.
  int entry_lifetime_ms_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestThrottlerEntry);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_
