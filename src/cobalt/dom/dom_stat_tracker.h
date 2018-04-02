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

  void OnHtmlElementCreated();
  void OnHtmlElementDestroyed();
  void OnHtmlElementInsertedIntoDocument();
  void OnHtmlElementRemovedFromDocument();
  void OnUpdateMatchingRules();
  void OnUpdateComputedStyle();
  void OnGenerateHtmlElementComputedStyle();
  void OnGeneratePseudoElementComputedStyle();
  void OnHtmlScriptElementExecuted(size_t script_size);
  void OnHtmlVideoElementPlaying();

  // This function updates the CVals from the periodic values and then clears
  // those values.
  void FlushPeriodicTracking();

  // Event-related
  void StartTrackingEvent();
  void StopTrackingEvent();

  int EventCountHtmlElement() const;
  int EventCountHtmlElementDocument() const;
  int event_count_html_element_created() const {
    return event_count_html_element_created_;
  }
  int event_count_html_element_destroyed() const {
    return event_count_html_element_destroyed_;
  }
  int event_count_html_element_document_added() const {
    return event_count_html_element_document_added_;
  }
  int event_count_html_element_document_removed() const {
    return event_count_html_element_document_removed_;
  }
  int event_count_update_matching_rules() const {
    return event_count_update_matching_rules_;
  }
  int event_count_update_computed_style() const {
    return event_count_update_computed_style_;
  }
  int event_count_generate_html_element_computed_style() const {
    return event_count_generate_html_element_computed_style_;
  }
  int event_count_generate_pseudo_element_computed_style() const {
    return event_count_generate_pseudo_element_computed_style_;
  }

  base::TimeDelta GetStopWatchTypeDuration(StopWatchType type) const;

 private:
  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const override;
  void OnStopWatchStopped(int id, base::TimeDelta time_elapsed) override;

  // Count cvals that are updated when the periodic tracking is flushed.
  base::CVal<int, base::CValPublic> count_html_element_;
  base::CVal<int, base::CValPublic> count_html_element_document_;

  // Periodic counts. The counts are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  int count_html_element_created_;
  int count_html_element_destroyed_;
  int count_html_element_document_added_;
  int count_html_element_document_removed_;

  // Count of HtmlScriptElement::Execute() calls, their total size in bytes, and
  // the time of last call.
  base::CVal<int, base::CValPublic> script_element_execute_count_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      script_element_execute_total_size_;
  base::CVal<int64, base::CValPublic> script_element_execute_time_;

  // Event-related
  bool is_tracking_event_;
  int event_initial_count_html_element_;
  int event_initial_count_html_element_document_;
  int event_count_html_element_created_;
  int event_count_html_element_destroyed_;
  int event_count_html_element_document_added_;
  int event_count_html_element_document_removed_;
  int event_count_update_matching_rules_;
  int event_count_update_computed_style_;
  int event_count_generate_html_element_computed_style_;
  int event_count_generate_pseudo_element_computed_style_;

  // Tracking of videos produced by an event.
  base::StopWatch event_video_start_delay_stop_watch_;
  base::CVal<base::TimeDelta, base::CValPublic> event_video_start_delay_;

  // Stop watch-related.
  std::vector<base::TimeDelta> stop_watch_durations_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_STAT_TRACKER_H_
