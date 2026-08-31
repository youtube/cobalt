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

#ifndef COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_ADAPTIVE_RESOURCE_SCHEDULER_H_
#define COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_ADAPTIVE_RESOURCE_SCHEDULER_H_

#include <memory>
#include <set>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cobalt/build/configs/buildflags.h"

namespace cobalt {

class CobaltResourceThrottle;

// Feature flag to control the Cobalt Adaptive Resource Scheduler.
BASE_DECLARE_FEATURE(kCobaltAdaptiveResourceScheduler);

// Manages TV interaction, startup, and playback states to coordinate network
// task scheduling in single-process Chrobalt.
//
// Threading: 100% thread-safe. Internal state is protected by base::Lock, and
// debounce checks use base::ThreadPool delayed tasks (zero sequence-bound
// timers).
class CobaltAdaptiveResourceScheduler {
 public:
  static CobaltAdaptiveResourceScheduler* GetInstance();

  CobaltAdaptiveResourceScheduler(const CobaltAdaptiveResourceScheduler&) =
      delete;
  CobaltAdaptiveResourceScheduler& operator=(
      const CobaltAdaptiveResourceScheduler&) = delete;

  // Returns true if the adaptive scheduler feature is enabled.
  bool IsEnabled() const;

  // Returns true if requests should currently be deferred (either starting up
  // or scrolling).
  bool ShouldDeferRequests();

  // Returns true if the application is currently in the cold startup phase.
  bool IsStartingUp();

  // Returns true if the user is actively scrolling/navigating or UI is
  // animating.
  bool IsScrolling();

  // Called when cold startup is complete (e.g. main frame rendered).
  void OnStartupCompleted();

#if !defined(OFFICIAL_BUILD)
  // Called when a visual frame is rendered after user interaction (non-official
  // builds, e.g. devel and qa).
  void OnVisualFrameRendered(base::TimeTicks key_time, bool success);
#endif

  // Called when a D-Pad key event or navigation input is detected.
  void OnUserInteraction(int key_code);

  // Explicitly notify the scheduler when a scroll animation starts or ends.
  void SetScrollState(bool is_scrolling);

  // Explicitly set startup state (useful for testing).
  void SetStartupStateForTesting(bool is_starting_up);

  // Register / Unregister URLLoaderThrottles
  void RegisterDeferredThrottle(CobaltResourceThrottle* throttle);
  void UnregisterDeferredThrottle(CobaltResourceThrottle* throttle);

  // Total count of currently deferred throttles (useful for testing & metrics).
  size_t GetDeferredCount();

  // For testing: override settle debounce duration.
  void SetSettleDelayForTesting(base::TimeDelta delay);

 private:
  friend class base::NoDestructor<CobaltAdaptiveResourceScheduler>;

  CobaltAdaptiveResourceScheduler();
  ~CobaltAdaptiveResourceScheduler();

  // Debounce check executed on thread pool after delay.
  void CheckScrollSettled(base::TimeTicks scheduled_time);

  // Drains deferred requests and throttles in a thread-safe and re-entrant safe
  // manner.
  void DrainDeferredQueue();

  mutable base::Lock lock_;

  bool is_starting_up_ GUARDED_BY(lock_) = true;
  bool is_scrolling_ GUARDED_BY(lock_) = false;
  base::TimeDelta settle_delay_ GUARDED_BY(lock_);
  base::TimeTicks last_interaction_time_ GUARDED_BY(lock_);

#if !defined(OFFICIAL_BUILD)
  // Frame timing & smoothness metrics during scrolling sessions (non-official
  // builds, e.g. devel and qa)
  size_t session_frame_count_ GUARDED_BY(lock_) = 0;
  size_t session_janky_frames_ GUARDED_BY(lock_) = 0;
  base::TimeDelta session_total_latency_ GUARDED_BY(lock_);
  base::TimeTicks session_start_time_ GUARDED_BY(lock_);
  base::TimeTicks session_last_frame_time_ GUARDED_BY(lock_);
#endif

  std::set<CobaltResourceThrottle*> deferred_throttles_ GUARDED_BY(lock_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_ADAPTIVE_RESOURCE_SCHEDULER_H_
