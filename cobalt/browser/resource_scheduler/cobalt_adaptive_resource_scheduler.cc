// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "cobalt/browser/resource_scheduler/cobalt_resource_throttle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace cobalt {

BASE_FEATURE(kCobaltAdaptiveResourceScheduler,
             "CobaltAdaptiveResourceScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
constexpr base::TimeDelta kDefaultScrollSettleDelay = base::Milliseconds(350);
constexpr base::TimeDelta kDefaultStartupFallbackTimeout = base::Seconds(10);
}  // namespace

// static
CobaltAdaptiveResourceScheduler*
CobaltAdaptiveResourceScheduler::GetInstance() {
  static base::NoDestructor<CobaltAdaptiveResourceScheduler> instance;
  return instance.get();
}

CobaltAdaptiveResourceScheduler::CobaltAdaptiveResourceScheduler()
    : settle_delay_(kDefaultScrollSettleDelay) {
  LOG(INFO) << "CobaltAdaptiveResourceScheduler: Initialized successfully. "
               "Feature enabled: "
            << IsEnabled();

  // 1. Post a BEST_EFFORT task to the UI/Renderer Main Thread.
  // In Chromium, BEST_EFFORT tasks on the UI thread will NOT run while
  // high-priority V8 JS compilation, bytecode generation, and synchronous DOM
  // construction tasks are active. The moment the main thread finishes
  // compiling and running the initial JS scripts and goes IDLE, this callback
  // executes immediately!
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce([]() {
                   LOG(INFO)
                       << "CobaltAdaptiveResourceScheduler: UI Main Thread "
                          "reached BEST_EFFORT Idle (JS compilation complete).";
                   CobaltAdaptiveResourceScheduler::GetInstance()
                       ->OnStartupCompleted();
                 }));

  // 2. Safety fallback using thread pool delayed task (10s safety net in case
  // of network errors).
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        LOG(INFO) << "CobaltAdaptiveResourceScheduler: 10s safety fallback "
                     "timer expired.";
        CobaltAdaptiveResourceScheduler::GetInstance()->OnStartupCompleted();
      }),
      kDefaultStartupFallbackTimeout);
}

CobaltAdaptiveResourceScheduler::~CobaltAdaptiveResourceScheduler() = default;

bool CobaltAdaptiveResourceScheduler::IsEnabled() const {
  return base::FeatureList::IsEnabled(kCobaltAdaptiveResourceScheduler);
}

bool CobaltAdaptiveResourceScheduler::ShouldDeferRequests() {
  base::AutoLock lock(lock_);
  return is_starting_up_ || is_scrolling_;
}

bool CobaltAdaptiveResourceScheduler::IsStartingUp() {
  base::AutoLock lock(lock_);
  return is_starting_up_;
}

bool CobaltAdaptiveResourceScheduler::IsScrolling() {
  base::AutoLock lock(lock_);
  return is_scrolling_;
}

void CobaltAdaptiveResourceScheduler::OnStartupCompleted() {
  {
    base::AutoLock lock(lock_);
    if (!is_starting_up_) {
      return;
    }
    is_starting_up_ = false;
    LOG(INFO) << "CobaltAdaptiveResourceScheduler: Startup completed. Draining "
                 "startup queue. "
              << "Deferred count: " << deferred_throttles_.size();
    if (is_scrolling_) {
      return;
    }
  }
  DrainDeferredQueue();
}

void CobaltAdaptiveResourceScheduler::SetStartupStateForTesting(
    bool is_starting_up) {
  {
    base::AutoLock lock(lock_);
    is_starting_up_ = is_starting_up;
    if (is_starting_up || is_scrolling_) {
      return;
    }
  }
  DrainDeferredQueue();
}

void CobaltAdaptiveResourceScheduler::OnUserInteraction(int key_code) {
  if (!IsEnabled()) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay;
  {
    base::AutoLock lock(lock_);
    if (!is_scrolling_) {
      LOG(INFO) << "CobaltAdaptiveResourceScheduler: Interaction detected (key "
                << key_code << "). Transitioning to SCROLLING state.";
    }
    is_scrolling_ = true;
    last_interaction_time_ = now;
    delay = settle_delay_;
  }

  // Post a thread-safe debounce check to the ThreadPool (NoDestructor
  // singleton, unretained is 100% safe).
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CobaltAdaptiveResourceScheduler::CheckScrollSettled,
                     base::Unretained(this), now),
      delay);
}

void CobaltAdaptiveResourceScheduler::CheckScrollSettled(
    base::TimeTicks scheduled_time) {
  {
    base::AutoLock lock(lock_);
    if (!is_scrolling_) {
      return;
    }
    // If another user interaction occurred after this task was scheduled, let
    // the later task handle it.
    if (last_interaction_time_ > scheduled_time) {
      return;
    }
    is_scrolling_ = false;
    LOG(INFO) << "CobaltAdaptiveResourceScheduler: Settle timer expired. "
                 "Transitioning to IDLE. "
              << "Draining " << deferred_throttles_.size()
              << " deferred requests.";
    if (is_starting_up_) {
      return;
    }
  }
  DrainDeferredQueue();
}

void CobaltAdaptiveResourceScheduler::SetScrollState(bool is_scrolling) {
  if (!IsEnabled()) {
    return;
  }

  LOG(INFO) << "CobaltAdaptiveResourceScheduler: SetScrollState("
            << is_scrolling << ")";
  if (is_scrolling) {
    base::AutoLock lock(lock_);
    is_scrolling_ = true;
  } else {
    CheckScrollSettled(base::TimeTicks::Now());
  }
}

void CobaltAdaptiveResourceScheduler::RegisterDeferredThrottle(
    CobaltResourceThrottle* throttle) {
  DCHECK(throttle);
  base::AutoLock lock(lock_);
  deferred_throttles_.insert(throttle);
  LOG(INFO) << "CobaltAdaptiveResourceScheduler: Registered deferred throttle. "
               "Total in queue: "
            << deferred_throttles_.size();
}

void CobaltAdaptiveResourceScheduler::UnregisterDeferredThrottle(
    CobaltResourceThrottle* throttle) {
  base::AutoLock lock(lock_);
  deferred_throttles_.erase(throttle);
}

size_t CobaltAdaptiveResourceScheduler::GetDeferredCount() {
  base::AutoLock lock(lock_);
  return deferred_throttles_.size();
}

void CobaltAdaptiveResourceScheduler::SetSettleDelayForTesting(
    base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  settle_delay_ = delay;
}

void CobaltAdaptiveResourceScheduler::DrainDeferredQueue() {
  std::vector<CobaltResourceThrottle*> throttles_to_resume;
  {
    base::AutoLock lock(lock_);
    while (!deferred_throttles_.empty()) {
      auto it = deferred_throttles_.begin();
      throttles_to_resume.push_back(*it);
      deferred_throttles_.erase(it);
    }
  }

  // Resume each throttle outside the lock.
  // CobaltResourceThrottle::ResumeLoading() safely handles sequence hopping to
  // the throttle's originating thread.
  for (auto* throttle : throttles_to_resume) {
    if (throttle) {
      throttle->ResumeLoading();
    }
  }
}

}  // namespace cobalt
