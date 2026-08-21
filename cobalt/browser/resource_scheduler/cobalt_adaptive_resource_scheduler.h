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
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace cobalt {

class CobaltResourceThrottle;
class CobaltProxyingURLLoaderFactory;

// Feature flag to control the Cobalt Adaptive Resource Scheduler.
BASE_DECLARE_FEATURE(kCobaltAdaptiveResourceScheduler);

// Manages TV interaction and playback states to coordinate network task
// scheduling in single-process Chrobalt.
//
// During active spatial navigation (e.g. D-pad carousel scrolling),
// non-critical requests (such as off-screen image thumbnails and background
// telemetry) are deferred to prevent socket futex locks and thread contention
// from starving Blink main JS and Compositor threads.
class CobaltAdaptiveResourceScheduler {
 public:
  static CobaltAdaptiveResourceScheduler* GetInstance();

  CobaltAdaptiveResourceScheduler(const CobaltAdaptiveResourceScheduler&) =
      delete;
  CobaltAdaptiveResourceScheduler& operator=(
      const CobaltAdaptiveResourceScheduler&) = delete;

  // Returns true if the adaptive scheduler feature is enabled.
  bool IsEnabled() const;

  // Returns true if the user is actively scrolling/navigating or UI is
  // animating.
  bool IsScrolling() const;

  // Called when a D-Pad key event or navigation input is detected.
  // Sets `is_scrolling_ = true` and restarts the settle debounce timer.
  void OnUserInteraction(int key_code);

  // Explicitly notify the scheduler when a scroll animation starts or ends.
  void SetScrollState(bool is_scrolling);

  // Register / Unregister URLLoaderThrottles
  void RegisterDeferredThrottle(CobaltResourceThrottle* throttle);
  void UnregisterDeferredThrottle(CobaltResourceThrottle* throttle);

  // Register / Unregister Proxying URLLoaderFactories
  void RegisterProxyFactory(CobaltProxyingURLLoaderFactory* factory);
  void UnregisterProxyFactory(CobaltProxyingURLLoaderFactory* factory);

  // Total count of currently deferred throttles (useful for testing & metrics).
  size_t GetDeferredCount() const;

  // For testing: override settle debounce duration.
  void SetSettleDelayForTesting(base::TimeDelta delay);

 private:
  friend class base::NoDestructor<CobaltAdaptiveResourceScheduler>;

  CobaltAdaptiveResourceScheduler();
  ~CobaltAdaptiveResourceScheduler();

  // Called when the settle debounce timer expires.
  void OnScrollSettled();

  // Drains deferred requests and throttles.
  void DrainDeferredQueue();

  SEQUENCE_CHECKER(sequence_checker_);

  bool is_scrolling_ = false;
  base::TimeDelta settle_delay_;
  base::OneShotTimer settle_timer_;

  std::set<CobaltResourceThrottle*> deferred_throttles_;
  std::set<CobaltProxyingURLLoaderFactory*> proxy_factories_;

  base::WeakPtrFactory<CobaltAdaptiveResourceScheduler> weak_ptr_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_ADAPTIVE_RESOURCE_SCHEDULER_H_
