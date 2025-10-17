// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetch_throttler.h"

#include <stdint.h>

#include <optional>

#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler_delegate.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace affiliations {

namespace {
constexpr auto kRetryableHttpErrors =
    base::MakeFixedFlatSet<net::HttpStatusCode>(
        {net::HTTP_INTERNAL_SERVER_ERROR, net::HTTP_BAD_GATEWAY,
         net::HTTP_SERVICE_UNAVAILABLE, net::HTTP_GATEWAY_TIMEOUT,
         net::HTTP_INSUFFICIENT_STORAGE,
         net::HTTP_NETWORK_AUTHENTICATION_REQUIRED});

// Checks if the response code belongs to |kRetryableHttpErrors|.
// Empty |response_code_optional| is treated as a retryable error.
bool IsHttpErrorNonRetryable(
    std::optional<net::HttpStatusCode> response_code_optional) {
  return response_code_optional
             ? !base::Contains(kRetryableHttpErrors,
                               response_code_optional.value())
             : false;
}
}  // namespace
// static
const net::BackoffEntry::Policy AffiliationFetchThrottler::kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before going into
    // exponential backoff.
    0,

    // Initial delay (in ms) once backoff starts.
    10 * 1000,  // 10 seconds

    // Factor by which the delay will be multiplied on each subsequent failure.
    4,

    // Fuzzing percentage: 50% will spread delays randomly between 50%--100% of
    // the nominal time.
    .5,  // 50%

    // Maximum delay (in ms) during exponential backoff.
    6 * 3600 * 1000,  // 6 hours

    // Time to keep an entry from being discarded even when it has no
    // significant state, -1 to never discard. (Not applicable.)
    -1,

    // False means that initial_delay_ms is the first delay once we start
    // exponential backoff, i.e., there is no delay after subsequent successful
    // requests.
    false,
};

// static
const int64_t AffiliationFetchThrottler::kGracePeriodAfterReconnectMs =
    10 * 1000;  // 10 seconds

// static
const int64_t AffiliationFetchThrottler::kBackoffAfterNonRetryableErrorHours =
    3;

AffiliationFetchThrottler::AffiliationFetchThrottler(
    AffiliationFetchThrottlerDelegate* delegate,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::TickClock* tick_clock)
    : delegate_(delegate),
      task_runner_(task_runner),
      network_connection_tracker_(network_connection_tracker),
      tick_clock_(tick_clock),
      state_(IDLE),
      has_network_connectivity_(false),
      is_fetch_scheduled_(false),
      exponential_backoff_(
          new net::BackoffEntry(&kBackoffPolicy, tick_clock_)) {
  DCHECK(delegate);
  // Start observing before querying the current connectivity state, so that if
  // the state changes concurrently in-between, it will not go unnoticed.
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

AffiliationFetchThrottler::~AffiliationFetchThrottler() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void AffiliationFetchThrottler::SignalNetworkRequestNeeded() {
  if (state_ != IDLE)
    return;

  state_ = FETCH_NEEDED;
  has_network_connectivity_ = !network_connection_tracker_->IsOffline();

  if (has_network_connectivity_)
    EnsureCallbackIsScheduled();
}

void AffiliationFetchThrottler::InformOfNetworkRequestComplete(
    bool success,
    std::optional<net::HttpStatusCode> http_status_code_optional) {
  DCHECK_EQ(state_, FETCH_IN_FLIGHT);
  state_ = IDLE;
  exponential_backoff_->InformOfRequest(success);

  if (!success && IsHttpErrorNonRetryable(http_status_code_optional)) {
    exponential_backoff_->SetCustomReleaseTime(
        std::max(exponential_backoff_->GetReleaseTime(),
                 tick_clock_->NowTicks() +
                     base::Hours(kBackoffAfterNonRetryableErrorHours)));
  }
}

bool AffiliationFetchThrottler::HasInternetConnection() const {
  return !network_connection_tracker_->IsOffline();
}

void AffiliationFetchThrottler::EnsureCallbackIsScheduled() {
  DCHECK_EQ(state_, FETCH_NEEDED);
  DCHECK(has_network_connectivity_);

  if (is_fetch_scheduled_)
    return;

  is_fetch_scheduled_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AffiliationFetchThrottler::OnBackoffDelayExpiredCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      exponential_backoff_->GetTimeUntilRelease());
}

void AffiliationFetchThrottler::OnBackoffDelayExpiredCallback() {
  DCHECK_EQ(state_, FETCH_NEEDED);
  DCHECK(is_fetch_scheduled_);
  is_fetch_scheduled_ = false;

  // Do nothing if network connectivity was lost while this callback was in the
  // task queue. The callback will be posted in the OnNetworkChanged
  // handler once again.
  if (!has_network_connectivity_)
    return;

  // The release time might have been increased if network connectivity was lost
  // and restored while this callback was in the task queue. If so, reschedule.
  if (exponential_backoff_->ShouldRejectRequest()) {
    EnsureCallbackIsScheduled();
  } else {
    state_ = delegate_->OnCanSendNetworkRequest() ? FETCH_IN_FLIGHT : IDLE;
  }
}

void AffiliationFetchThrottler::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  bool old_has_network_connectivity = has_network_connectivity_;
  // We reread the connection type here instead of relying on |type| because
  // NetworkConnectionTracker will call this function an extra time with
  // CONNECTION_NONE whenever the connection changes to an online state, even
  // if it was already in a different online state.
  has_network_connectivity_ = !network_connection_tracker_->IsOffline();

  // Only react when network connectivity has been reestablished.
  if (!has_network_connectivity_ || old_has_network_connectivity)
    return;

  double grace_ms = kGracePeriodAfterReconnectMs *
                    (1 - base::RandDouble() * kBackoffPolicy.jitter_factor);
  exponential_backoff_->SetCustomReleaseTime(
      std::max(exponential_backoff_->GetReleaseTime(),
               tick_clock_->NowTicks() + base::Milliseconds(grace_ms)));

  if (state_ == FETCH_NEEDED)
    EnsureCallbackIsScheduled();
}

}  // namespace affiliations
