// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the
// License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in
// writing, software distributed under the License is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See
// the License for the specific language governing
// permissions and limitations under the License.

#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"

namespace cobalt {

BASE_FEATURE(kCobaltAdaptiveResourceScheduler,
             "CobaltAdaptiveResourceScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
constexpr base::TimeDelta kDefaultScrollSettleDelay = base::Milliseconds(350);
}  // namespace

// static
CobaltAdaptiveResourceScheduler*
CobaltAdaptiveResourceScheduler::GetInstance() {
  static base::NoDestructor<CobaltAdaptiveResourceScheduler> instance;
  return instance.get();
}

CobaltAdaptiveResourceScheduler::CobaltAdaptiveResourceScheduler()
    : settle_delay_(kDefaultScrollSettleDelay) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "CobaltAdaptiveResourceScheduler: "
                "Initialized successfully. "
                "Feature enabled: "
             << IsEnabled();
}

CobaltAdaptiveResourceScheduler::~CobaltAdaptiveResourceScheduler() = default;

bool CobaltAdaptiveResourceScheduler::IsEnabled() const {
  return base::FeatureList::IsEnabled(kCobaltAdaptiveResourceScheduler);
}

bool CobaltAdaptiveResourceScheduler::IsScrolling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_scrolling_;
}

void CobaltAdaptiveResourceScheduler::OnUserInteraction(int key_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    return;
  }

  if (!is_scrolling_) {
    DLOG(INFO) << "CobaltAdaptiveResourceScheduler: "
                  "Interaction detected (key "
               << key_code << "). Transitioning to SCROLLING state.";
  }
  is_scrolling_ = true;

  // Restart the debounce timer.
  settle_timer_.Stop();
  settle_timer_.Start(
      FROM_HERE, settle_delay_,
      base::BindOnce(&CobaltAdaptiveResourceScheduler::OnScrollSettled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CobaltAdaptiveResourceScheduler::SetScrollState(bool is_scrolling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    return;
  }

  DLOG(INFO) << "CobaltAdaptiveResourceScheduler: SetScrollState("
             << is_scrolling << ")";
  if (is_scrolling) {
    is_scrolling_ = true;
    settle_timer_.Stop();
  } else {
    OnScrollSettled();
  }
}

void CobaltAdaptiveResourceScheduler::RegisterDeferredThrottle(
    CobaltResourceThrottle* throttle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(throttle);
  deferred_throttles_.insert(throttle);
  DLOG(INFO) << "CobaltAdaptiveResourceScheduler: "
                "Registered deferred throttle. "
                "Total in queue: "
             << deferred_throttles_.size();
}

void CobaltAdaptiveResourceScheduler::UnregisterDeferredThrottle(
    CobaltResourceThrottle* throttle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  deferred_throttles_.erase(throttle);
}

size_t CobaltAdaptiveResourceScheduler::GetDeferredCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return deferred_throttles_.size();
}

void CobaltAdaptiveResourceScheduler::SetSettleDelayForTesting(
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settle_delay_ = delay;
}

void CobaltAdaptiveResourceScheduler::OnScrollSettled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_scrolling_ = false;
  DLOG(INFO) << "CobaltAdaptiveResourceScheduler: Settle "
                "timer expired. "
                "Transitioning to IDLE. "
             << "Draining " << deferred_throttles_.size()
             << " deferred requests.";
  DrainDeferredQueue();
}

void CobaltAdaptiveResourceScheduler::DrainDeferredQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pop throttles one-by-one from the set to avoid
  // re-entrancy use-after-free bugs if resuming a throttle
  // causes another throttle to be cancelled and destroyed.
  while (!deferred_throttles_.empty()) {
    auto it = deferred_throttles_.begin();
    CobaltResourceThrottle* throttle = *it;
    deferred_throttles_.erase(it);
    if (throttle) {
      throttle->ResumeLoading();
    }
  }
}

}  // namespace cobalt
