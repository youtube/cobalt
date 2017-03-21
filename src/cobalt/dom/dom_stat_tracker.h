// Copyright 2016 Google Inc. All Rights Reserved.
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
    kStopWatchTypeRunAnimationFrameCallbacks,
    kStopWatchTypeUpdateComputedStyle,
    kStopWatchTypeEventVideoStartDelay,
    kNumStopWatchTypes,
  };

  explicit DomStatTracker(const std::string& name);
  ~DomStatTracker();

  // Event-related
  void OnStartEvent();
  void OnEndEvent();

  void OnHtmlVideoElementPlaying();

  // Periodic count-related
  void OnHtmlElementCreated();
  void OnHtmlElementDestroyed();
  void OnUpdateMatchingRules();
  void OnUpdateComputedStyle();
  void OnGenerateHtmlElementComputedStyle();
  void OnGeneratePseudoElementComputedStyle();

  int total_html_elements() const { return total_html_elements_; }

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
  int generate_html_element_computed_style_count() const {
    return generate_html_element_computed_style_count_;
  }
  int generate_pseudo_element_computed_style_count() const {
    return generate_pseudo_element_computed_style_count_;
  }

  base::TimeDelta GetStopWatchTypeDuration(StopWatchType type) const;

 private:
  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const OVERRIDE;
  void OnStopWatchStopped(int id, base::TimeDelta time_elapsed) OVERRIDE;

  // This function updates the CVals from the periodic values and then clears
  // those values.
  void FlushPeriodicTracking();

  // Count cvals that are updated when the periodic tracking is flushed.
  base::CVal<int, base::CValPublic> total_html_elements_;

  // Event-related
  bool is_event_active_;

  // Tracking of videos produced by an event.
  base::StopWatch event_video_start_delay_stop_watch_;
  base::CVal<base::TimeDelta> event_video_start_delay_;

  // Periodic counts. The counts are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  int html_elements_created_count_;
  int html_elements_destroyed_count_;
  int update_matching_rules_count_;
  int update_computed_style_count_;
  int generate_html_element_computed_style_count_;
  int generate_pseudo_element_computed_style_count_;

  // Stop watch-related.
  std::vector<base::TimeDelta> stop_watch_durations_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_STAT_TRACKER_H_
