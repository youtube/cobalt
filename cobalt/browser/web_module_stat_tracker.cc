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

#include "cobalt/browser/web_module_stat_tracker.h"

#include "base/stringprintf.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"

// The maximum allowed string size of any recorded stat
const std::string::size_type kMaxRecordedStatsBytes = 64 * 1024;

namespace cobalt {
namespace browser {

WebModuleStatTracker::WebModuleStatTracker(const std::string& name,
                                           bool should_track_event_stats)
    : dom_stat_tracker_(new dom::DomStatTracker(name)),
      layout_stat_tracker_(new layout::LayoutStatTracker(name)),
      should_track_event_stats_(should_track_event_stats),
      current_event_type_(kEventTypeInvalid) {
  if (should_track_event_stats_) {
    event_stats_.reserve(kNumEventTypes);
    for (int i = 0; i < kNumEventTypes; ++i) {
      EventType event_type = static_cast<EventType>(i);
      event_stats_.push_back(new EventStats(StringPrintf(
          "%s.%s", name.c_str(), GetEventTypeName(event_type).c_str())));
    }
  }

  stop_watches_.reserve(kNumStopWatchTypes);
  for (int i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watches_.push_back(
        base::StopWatch(i, base::StopWatch::kAutoStartOff, this));
  }
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

WebModuleStatTracker::~WebModuleStatTracker() { EndCurrentEvent(false); }

void WebModuleStatTracker::OnInjectEvent(
    const scoped_refptr<dom::Event>& event) {
  if (!should_track_event_stats_) {
    return;
  }

  EndCurrentEvent(false);

  if (event->type() == base::Tokens::keydown()) {
    current_event_type_ = kEventTypeKeyDown;
  } else if (event->type() == base::Tokens::keyup()) {
    current_event_type_ = kEventTypeKeyUp;
  } else {
    current_event_type_ = kEventTypeInvalid;
  }

  // If this is a valid event, then all of the timers are now started/enabled.
  // They will continue running for this event until either another event is
  // injected, or a render tree is produced for this event.
  if (current_event_type_ != kEventTypeInvalid) {
    stop_watches_[kStopWatchTypeEvent].Start();
    dom_stat_tracker_->EnableStopWatches();
    layout_stat_tracker_->EnableStopWatches();
  }
}

void WebModuleStatTracker::OnRenderTreeProduced() {
  EndCurrentEvent(true);

  // Counts are flushed after new render trees are produced.
  dom_stat_tracker_->FlushPeriodicTracking();
  layout_stat_tracker_->FlushPeriodicTracking();
}

void WebModuleStatTracker::OnSetRecordStats(bool set) {
  record_stats_ = set;

  // Every time this variable is set, we clear out our stats
  for (ScopedVector<EventStats>::iterator it = event_stats_.begin();
       it != event_stats_.end();
       ++it) {
    (*it)->event_durations = "[]";
  }
}

WebModuleStatTracker::EventStats::EventStats(const std::string& name)
    : count_dom_html_elements_created(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Created", name.c_str()),
          0, "Number of HTML elements created."),
      count_dom_html_elements_destroyed(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.Destroyed",
                       name.c_str()),
          0, "Number of HTML elements destroyed."),
      count_dom_update_matching_rules(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.UpdateMatchingRules",
                       name.c_str()),
          0, "Number of update matching rules for HTML elements."),
      count_dom_update_computed_style(
          StringPrintf("Event.Count.%s.DOM.HtmlElement.UpdateComputedStyle",
                       name.c_str()),
          0, "Number of update computed styles for HTML elements."),
      count_layout_boxes_created(
          StringPrintf("Event.Count.%s.Layout.Box.Created", name.c_str()), 0,
          "Number of boxes created."),
      count_layout_boxes_destroyed(
          StringPrintf("Event.Count.%s.Layout.Box.Destroyed", name.c_str()), 0,
          "Number of boxes destroyed."),
      duration_total(StringPrintf("Event.Duration.%s", name.c_str()),
                     base::TimeDelta(),
                     "Total duration of the event (in microseconds). This is "
                     "the time elapsed from the event injection until the "
                     "render tree is produced."),
      duration_dom_inject_event(
          StringPrintf("Event.Duration.%s.DOM.InjectEvent", name.c_str()),
          base::TimeDelta(),
          "Injection duration, which includes JS, for event (in "
          "microseconds). This does not include subsequent DOM and Layout "
          "processing."),
      duration_dom_update_computed_style(
          StringPrintf("Event.Duration.%s.DOM.UpdateComputedStyle",
                       name.c_str()),
          base::TimeDelta(),
          "UpdateComputedStyle duration for event (in microseconds)."),
      duration_layout_box_tree(
          StringPrintf("Event.Duration.%s.Layout.BoxTree", name.c_str()),
          base::TimeDelta(),
          "Layout box tree duration for event (in microseconds)."),
      duration_layout_box_generation(
          StringPrintf("Event.Duration.%s.Layout.BoxTree.BoxGeneration",
                       name.c_str()),
          base::TimeDelta(),
          "BoxGeneration duration for event (in microseconds)."),
      duration_layout_update_used_sizes(
          StringPrintf("Event.Duration.%s.Layout.BoxTree.UpdateUsedSizes",
                       name.c_str()),
          base::TimeDelta(),
          "UpdateUsedSizes duration for event (in microseconds)."),
      duration_layout_render_and_animate(
          StringPrintf("Event.Duration.%s.Layout.RenderAndAnimate",
                       name.c_str()),
          base::TimeDelta(),
          "RenderAndAnimate duration for event (in microseconds)."),
      event_durations(StringPrintf("Event.Durations.%s", name.c_str()),
                     "[]",
                     "JSON array of all event durations (in microseconds) "
                     "since reset.") {}

bool WebModuleStatTracker::IsStopWatchEnabled(int /*id*/) const { return true; }

void WebModuleStatTracker::OnStopWatchStopped(int id,
                                              base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

void WebModuleStatTracker::EndCurrentEvent(bool was_render_tree_produced) {
  if (current_event_type_ == kEventTypeInvalid) {
    return;
  }

  stop_watch_durations_[kStopWatchTypeEvent] = base::TimeDelta();
  stop_watches_[kStopWatchTypeEvent].Stop();
  dom_stat_tracker_->DisableStopWatches();
  layout_stat_tracker_->DisableStopWatches();

  EventStats* event_stats = event_stats_[current_event_type_];

  // Update event counts
  event_stats->count_dom_html_elements_created =
      dom_stat_tracker_->html_elements_created_count();
  event_stats->count_dom_html_elements_destroyed =
      dom_stat_tracker_->html_elements_destroyed_count();
  event_stats->count_dom_update_matching_rules =
      dom_stat_tracker_->update_matching_rules_count();
  event_stats->count_dom_update_computed_style =
      dom_stat_tracker_->update_computed_style_count();
  event_stats->count_layout_boxes_created =
      layout_stat_tracker_->boxes_created_count();
  event_stats->count_layout_boxes_destroyed =
      layout_stat_tracker_->boxes_destroyed_count();

  // Update event durations
  base::TimeDelta event_injection_duration =
      dom_stat_tracker_->GetStopWatchTypeDuration(
          dom::DomStatTracker::kStopWatchTypeInjectEvent);
  // If a render tree was produced, then the total duration is the duration from
  // when the event started until now. Otherwise, the injection duration is
  // used. This is because some events do not trigger a new layout. In these
  // cases, using the duration from the start of the event until now is
  // misleading as it merely indicates how long the user waited to initiate the
  // next event. When this occurs, the injection duration provides a much more
  // accurate picture of how long the event takes.
  base::TimeDelta duration_total = was_render_tree_produced
                                    ? stop_watch_durations_[kStopWatchTypeEvent]
                                    : event_injection_duration;
  event_stats->duration_total = duration_total;

  if (record_stats_) {
    std::string prev_durations = event_stats->event_durations.value();
    if (prev_durations.size() <= 2) {
      event_stats->event_durations =
          StringPrintf("[%ld]", duration_total.InMicroseconds());
    } else if (prev_durations.size() < kMaxRecordedStatsBytes) {
      event_stats->event_durations
          = StringPrintf("%s,%ld]",
                         prev_durations.substr(
                             0, prev_durations.size() - 1).c_str(),
                             duration_total.InMicroseconds());
    }
  }

  event_stats->duration_dom_inject_event = event_injection_duration;
  event_stats->duration_dom_update_computed_style =
      dom_stat_tracker_->GetStopWatchTypeDuration(
          dom::DomStatTracker::kStopWatchTypeUpdateComputedStyle);
  event_stats->duration_layout_box_tree =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeLayoutBoxTree);
  event_stats->duration_layout_box_generation =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeBoxGeneration);
  event_stats->duration_layout_update_used_sizes =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeUpdateUsedSizes);
  event_stats->duration_layout_render_and_animate =
      layout_stat_tracker_->GetStopWatchTypeDuration(
          layout::LayoutStatTracker::kStopWatchTypeRenderAndAnimate);

  current_event_type_ = kEventTypeInvalid;
}

std::string WebModuleStatTracker::GetEventTypeName(
    WebModuleStatTracker::EventType event_type) {
  switch (event_type) {
    case WebModuleStatTracker::kEventTypeKeyDown:
      return "KeyDown";
    case WebModuleStatTracker::kEventTypeKeyUp:
      return "KeyUp";
    case WebModuleStatTracker::kEventTypeInvalid:
    case WebModuleStatTracker::kNumEventTypes:
    default:
      NOTREACHED();
      return "Invalid";
  }
}

}  // namespace browser
}  // namespace cobalt
