/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DOM_DOM_STAT_TRACKER_H_
#define COBALT_DOM_DOM_STAT_TRACKER_H_

#include <string>
#include <vector>

#include "cobalt/base/c_val.h"
#include "cobalt/base/stop_watch.h"

namespace cobalt {
namespace dom {

// This class handles tracking DOM stats and is intended for single thread use.
class DomStatTracker : public base::StopWatchOwner {
 public:
  enum StopWatchType {
    kStopWatchTypeInjectEvent,
    kStopWatchTypeUpdateComputedStyle,
    kNumStopWatchTypes,
  };

  explicit DomStatTracker(const std::string& name);
  ~DomStatTracker();

  // This function updates the CVals from the periodic values and then clears
  // those values.
  void FlushPeriodicTracking();

  // Periodic count-related
  void OnHtmlElementCreated();
  void OnHtmlElementDestroyed();
  void OnUpdateMatchingRules();
  void OnUpdateComputedStyle();

  int html_elements_created_count() const {
    return html_elements_created_count_;
  }
  int html_elements_destroyed_count() const {
    return html_elements_destroyed_count_;
  }
  int update_matching_rules_count() const {
    return update_matching_rules_count_;
  }
  int update_computed_style_count() const {
    return update_computed_style_count_;
  }

  // Stop watch-related
  void EnableStopWatches();
  void DisableStopWatches();

  int64 GetStopWatchTypeTime(StopWatchType type) const;

 private:
  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const OVERRIDE;
  void OnStopWatchStopped(int id, int64 time_elapsed) OVERRIDE;

  // CVals. They are updated when the periodic counts are flushed.
  base::PublicCVal<int> total_html_elements_;

  // Periodic counts. The counts are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  int html_elements_created_count_;
  int html_elements_destroyed_count_;
  int update_matching_rules_count_;
  int update_computed_style_count_;

  // Stop watch-related. The times are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  bool are_stop_watches_enabled_;
  std::vector<int64> stop_watch_times_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_STAT_TRACKER_H_
