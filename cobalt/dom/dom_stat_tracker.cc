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

#include "cobalt/dom/dom_stat_tracker.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace dom {

DomStatTracker::DomStatTracker(const std::string& name)
    : html_elements_count_(
          StringPrintf("Count.%s.DOM.HtmlElement.Total", name.c_str()), 0,
          "Total number of HTML elements."),
      document_html_elements_count_(
          StringPrintf("Count.%s.DOM.HtmlElement.Document", name.c_str()), 0,
          "Number of HTML elements in the document."),
      is_event_active_(false),
      event_video_start_delay_stop_watch_(kStopWatchTypeEventVideoStartDelay,
                                          base::StopWatch::kAutoStartOff, this),
      event_video_start_delay_(
          StringPrintf("Event.Duration.%s.DOM.VideoStartDelay", name.c_str()),
          base::TimeDelta(), "Total delay between event and video starting."),
      script_element_execute_count_(
          StringPrintf("Count.%s.DOM.HtmlScriptElement.Execute", name.c_str()),
          0, "Count of HTML script element execute calls."),
      script_element_execute_time_(
          StringPrintf("Time.%s.DOM.HtmlScriptElement.Execute", name.c_str()),
          0, "Time of the last HTML script element execute."),
      html_elements_created_count_(0),
      html_elements_destroyed_count_(0),
      html_elements_inserted_into_document_count_(0),
      html_elements_removed_from_document_count_(0),
      update_matching_rules_count_(0),
      update_computed_style_count_(0),
      generate_html_element_computed_style_count_(0),
      generate_pseudo_element_computed_style_count_(0) {
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

DomStatTracker::~DomStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the elements were removed from the document and
  // destroyed.
  DCHECK_EQ(html_elements_count_, 0);
  DCHECK_EQ(document_html_elements_count_, 0);

  event_video_start_delay_stop_watch_.Stop();
}

void DomStatTracker::OnStartEvent() {
  is_event_active_ = true;

  // Flush the periodic tracking prior to starting the event. This ensures that
  // an accurate count of the periodic counts is produced during the event.
  FlushPeriodicTracking();

  // Stop the watch before re-starting it. This ensures that the count is
  // zeroed out if it was still running from the prior event (this is a common
  // case as most events do not result in a video starting).
  event_video_start_delay_stop_watch_.Stop();
  event_video_start_delay_stop_watch_.Start();
  event_video_start_delay_ = base::TimeDelta();

  // Zero out the stop watch durations so that the numbers will only include the
  // event.
  for (size_t i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watch_durations_[i] = base::TimeDelta();
  }
}

void DomStatTracker::OnEndEvent() {
  is_event_active_ = false;

  // Flush the periodic tracking after the event. This updates the cval totals,
  // providing an accurate picture of them at the moment the event ends.
  FlushPeriodicTracking();
}

void DomStatTracker::OnHtmlVideoElementPlaying() {
  if (event_video_start_delay_stop_watch_.IsCounting()) {
    event_video_start_delay_stop_watch_.Stop();
    event_video_start_delay_ =
        stop_watch_durations_[kStopWatchTypeEventVideoStartDelay];
  }
}

void DomStatTracker::OnHtmlScriptElementExecuted() {
  ++script_element_execute_count_;
  script_element_execute_time_ = base::TimeTicks::Now().ToInternalValue();
}

void DomStatTracker::OnHtmlElementCreated() { ++html_elements_created_count_; }

void DomStatTracker::OnHtmlElementDestroyed() {
  ++html_elements_destroyed_count_;
}

void DomStatTracker::OnHtmlElementInsertedIntoDocument() {
  ++html_elements_inserted_into_document_count_;
}

void DomStatTracker::OnHtmlElementRemovedFromDocument() {
  ++html_elements_removed_from_document_count_;
}

void DomStatTracker::OnUpdateMatchingRules() { ++update_matching_rules_count_; }

void DomStatTracker::OnUpdateComputedStyle() { ++update_computed_style_count_; }

void DomStatTracker::OnGenerateHtmlElementComputedStyle() {
  ++generate_html_element_computed_style_count_;
}

void DomStatTracker::OnGeneratePseudoElementComputedStyle() {
  ++generate_pseudo_element_computed_style_count_;
}

base::TimeDelta DomStatTracker::GetStopWatchTypeDuration(
    StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool DomStatTracker::IsStopWatchEnabled(int id) const {
  return is_event_active_ || id == kStopWatchTypeEventVideoStartDelay;
}

void DomStatTracker::OnStopWatchStopped(int id, base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

void DomStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  html_elements_count_ +=
      html_elements_created_count_ - html_elements_destroyed_count_;
  document_html_elements_count_ += html_elements_inserted_into_document_count_ -
                                   html_elements_removed_from_document_count_;

  // Now clear the values.
  html_elements_created_count_ = 0;
  html_elements_destroyed_count_ = 0;
  html_elements_inserted_into_document_count_ = 0;
  html_elements_removed_from_document_count_ = 0;
  update_matching_rules_count_ = 0;
  update_computed_style_count_ = 0;
  generate_html_element_computed_style_count_ = 0;
  generate_pseudo_element_computed_style_count_ = 0;
}

}  // namespace dom
}  // namespace cobalt
