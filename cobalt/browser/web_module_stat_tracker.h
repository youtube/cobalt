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

#ifndef COBALT_BROWSER_WEB_MODULE_STAT_TRACKER_H_
#define COBALT_BROWSER_WEB_MODULE_STAT_TRACKER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/stop_watch.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/event.h"
#include "cobalt/layout/layout_stat_tracker.h"

namespace cobalt {
namespace browser {

// This class handles tracking web module stats and is intended for single
// thread use. It also owns the |DomStatTracker| and |LayoutStatTracker|.
class WebModuleStatTracker : public base::StopWatchOwner {
 public:
  WebModuleStatTracker(const std::string& web_module_name,
                       bool should_track_dispatched_events);

  dom::DomStatTracker* dom_stat_tracker() const {
    return dom_stat_tracker_.get();
  }

  layout::LayoutStatTracker* layout_stat_tracker() const {
    return layout_stat_tracker_.get();
  }

  // |OnStartDispatchEvent| starts event stat tracking if
  // |should_track_dispatched_events_| is true. Otherwise, it does nothing.
  void OnStartDispatchEvent(const scoped_refptr<dom::Event>& event);

  // |OnStopDispatchEvent| notifies the event stat tracking that the event has
  // finished being dispatched. If no animation frame callbacks and also no
  // render tree is pending, it also ends tracking of the event.
  void OnStopDispatchEvent(bool are_animation_frame_callbacks_pending,
                           bool is_new_render_tree_pending);

  // |OnRanAnimationFrameCallbacks| ends stat tracking for the current event
  // if no render tree is pending.
  void OnRanAnimationFrameCallbacks(bool is_new_render_tree_pending);

  // |OnRenderTreeProduced| ends stat tracking for the current event.
  void OnRenderTreeProduced(const base::TimeTicks& produced_time);

  // |OnRenderTreeRasterized| ends stat tracking for the current event.
  void OnRenderTreeRasterized(const base::TimeTicks& produced_time,
                              const base::TimeTicks& rasterized_time);

  // Returns whether or not an event-based render tree has been produced but not
  // yet rasterized.
  bool IsRenderTreeRasterizationPending() const;

 private:
  enum EventType {
    kEventTypeInvalid = -1,
    kEventTypeKeyDown,
    kEventTypeKeyUp,
    kEventTypePointerDown,
    kEventTypePointerUp,
    kNumEventTypes,
  };

  enum StopWatchType {
    kStopWatchTypeDispatchEvent,
    kNumStopWatchTypes,
  };

  struct EventStats {
    explicit EventStats(const std::string& name);

    base::CVal<bool, base::CValPublic> produced_render_tree;

    // Count-related
    base::CVal<int, base::CValPublic> count_dom_html_element;
    base::CVal<int, base::CValPublic> count_dom_html_element_created;
    base::CVal<int, base::CValPublic> count_dom_html_element_destroyed;
    base::CVal<int, base::CValPublic> count_dom_html_element_document;
    base::CVal<int, base::CValPublic> count_dom_html_element_document_added;
    base::CVal<int, base::CValPublic> count_dom_html_element_document_removed;
    base::CVal<int, base::CValPublic> count_dom_update_matching_rules;
    base::CVal<int, base::CValPublic> count_dom_update_computed_style;
    base::CVal<int, base::CValPublic>
        count_dom_generate_html_element_computed_style;
    base::CVal<int, base::CValPublic>
        count_dom_generate_pseudo_element_computed_style;
    base::CVal<int, base::CValPublic> count_layout_box;
    base::CVal<int, base::CValPublic> count_layout_box_created;
    base::CVal<int, base::CValPublic> count_layout_box_destroyed;
    base::CVal<int, base::CValPublic> count_layout_update_size;
    base::CVal<int, base::CValPublic> count_layout_render_and_animate;
    base::CVal<int, base::CValPublic> count_layout_update_cross_references;

    // Duration-related
    base::CVal<base::TimeDelta, base::CValPublic> duration_total;
    base::CVal<base::TimeDelta, base::CValPublic> duration_dom_dispatch_event;
    base::CVal<base::TimeDelta, base::CValPublic>
        duration_dom_run_animation_frame_callbacks;
    base::CVal<base::TimeDelta, base::CValPublic>
        duration_dom_update_computed_style;
    base::CVal<base::TimeDelta, base::CValPublic> duration_layout_box_tree;
    base::CVal<base::TimeDelta, base::CValPublic>
        duration_layout_box_generation;
    base::CVal<base::TimeDelta, base::CValPublic>
        duration_layout_update_used_sizes;
    base::CVal<base::TimeDelta, base::CValPublic>
        duration_layout_render_and_animate;
    base::CVal<base::TimeDelta, base::CValPublic> duration_renderer_rasterize;

#if defined(ENABLE_WEBDRIVER)
    // A string containing all of the event's values, excluding post-event
    // delays, as a dictionary of key-value pairs. This is used by the Webdriver
    // and is only enabled with it.
    base::CVal<std::string> value_dictionary;
#endif  // ENABLE_WEBDRIVER
  };

  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const override;
  void OnStopWatchStopped(int id, base::TimeDelta time_elapsed) override;

  // End the current event if one is active. This triggers an update of all
  // |EventStats| for the event.
  void EndCurrentEvent(base::TimeTicks end_time);

  static std::string GetEventTypeName(EventType event_type);

  const std::string name_;
  const bool should_track_event_stats_;

  // Web module owns the dom and layout stat trackers.
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  scoped_ptr<layout::LayoutStatTracker> layout_stat_tracker_;

  // Event-related
  base::CVal<bool> event_is_processing_;

  EventType current_event_type_;
  base::TimeTicks current_event_start_time_;
  base::TimeTicks current_event_render_tree_produced_time_;

  // Each individual |EventType| has its own entry in the vector.
  ScopedVector<EventStats> event_stats_list_;

  // Stop watch-related
  std::vector<base::StopWatch> stop_watches_;
  std::vector<base::TimeDelta> stop_watch_durations_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_WEB_MODULE_STAT_TRACKER_H_
