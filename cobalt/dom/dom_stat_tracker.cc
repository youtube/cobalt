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
    : count_html_element_(
          StringPrintf("Count.%s.DOM.HtmlElement", name.c_str()), 0,
          "Total number of HTML elements."),
      count_html_element_document_(
          StringPrintf("Count.%s.DOM.HtmlElement.Document", name.c_str()), 0,
          "Number of HTML elements in the document."),
      count_html_element_created_(0),
      count_html_element_destroyed_(0),
      count_html_element_document_added_(0),
      count_html_element_document_removed_(0),
      script_element_execute_count_(
          StringPrintf("Count.%s.DOM.HtmlScriptElement.Execute", name.c_str()),
          0, "Count of HTML script element execute calls."),
      script_element_execute_time_(
          StringPrintf("Time.%s.DOM.HtmlScriptElement.Execute", name.c_str()),
          0, "Time of the last HTML script element execute."),
      is_tracking_event_(false),
      event_initial_count_html_element_(0),
      event_initial_count_html_element_document_(0),
      event_count_html_element_created_(0),
      event_count_html_element_destroyed_(0),
      event_count_html_element_document_added_(0),
      event_count_html_element_document_removed_(0),
      event_count_update_matching_rules_(0),
      event_count_update_computed_style_(0),
      event_count_generate_html_element_computed_style_(0),
      event_count_generate_pseudo_element_computed_style_(0),
      event_video_start_delay_stop_watch_(kStopWatchTypeEventVideoStartDelay,
                                          base::StopWatch::kAutoStartOff, this),
      event_video_start_delay_(
          StringPrintf("Event.Duration.%s.DOM.VideoStartDelay", name.c_str()),
          base::TimeDelta(), "Total delay between event and video starting.") {
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

DomStatTracker::~DomStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the elements were removed from the document and
  // destroyed.
  DCHECK_EQ(count_html_element_, 0);
  DCHECK_EQ(count_html_element_document_, 0);

  event_video_start_delay_stop_watch_.Stop();
}

void DomStatTracker::OnHtmlElementCreated() {
  ++count_html_element_created_;
  if (is_tracking_event_) {
    ++event_count_html_element_created_;
  }
}

void DomStatTracker::OnHtmlElementDestroyed() {
  ++count_html_element_destroyed_;
  if (is_tracking_event_) {
    ++event_count_html_element_destroyed_;
  }
}

void DomStatTracker::OnHtmlElementInsertedIntoDocument() {
  ++count_html_element_document_added_;
  if (is_tracking_event_) {
    ++event_count_html_element_document_added_;
  }
}

void DomStatTracker::OnHtmlElementRemovedFromDocument() {
  ++count_html_element_document_removed_;
  if (is_tracking_event_) {
    ++event_count_html_element_document_removed_;
  }
}

void DomStatTracker::OnUpdateMatchingRules() {
  if (is_tracking_event_) {
    ++event_count_update_matching_rules_;
  }
}

void DomStatTracker::OnUpdateComputedStyle() {
  if (is_tracking_event_) {
    ++event_count_update_computed_style_;
  }
}

void DomStatTracker::OnGenerateHtmlElementComputedStyle() {
  if (is_tracking_event_) {
    ++event_count_generate_html_element_computed_style_;
  }
}

void DomStatTracker::OnGeneratePseudoElementComputedStyle() {
  if (is_tracking_event_) {
    ++event_count_generate_pseudo_element_computed_style_;
  }
}

void DomStatTracker::OnHtmlScriptElementExecuted() {
  ++script_element_execute_count_;
  script_element_execute_time_ = base::TimeTicks::Now().ToInternalValue();
}

void DomStatTracker::OnHtmlVideoElementPlaying() {
  if (event_video_start_delay_stop_watch_.IsCounting()) {
    event_video_start_delay_stop_watch_.Stop();
    event_video_start_delay_ =
        stop_watch_durations_[kStopWatchTypeEventVideoStartDelay];
  }
}

void DomStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  count_html_element_ +=
      count_html_element_created_ - count_html_element_destroyed_;
  count_html_element_document_ +=
      count_html_element_document_added_ - count_html_element_document_removed_;

  // Now clear the values.
  count_html_element_created_ = 0;
  count_html_element_destroyed_ = 0;
  count_html_element_document_added_ = 0;
  count_html_element_document_removed_ = 0;
}

void DomStatTracker::StartTrackingEvent() {
  DCHECK(!is_tracking_event_);
  is_tracking_event_ = true;

  event_initial_count_html_element_ = count_html_element_ +
                                      count_html_element_created_ -
                                      count_html_element_destroyed_;
  event_initial_count_html_element_document_ =
      count_html_element_document_ + count_html_element_document_added_ -
      count_html_element_document_removed_;

  event_count_html_element_created_ = 0;
  event_count_html_element_destroyed_ = 0;
  event_count_html_element_document_added_ = 0;
  event_count_html_element_document_removed_ = 0;
  event_count_update_matching_rules_ = 0;
  event_count_update_computed_style_ = 0;
  event_count_generate_html_element_computed_style_ = 0;
  event_count_generate_pseudo_element_computed_style_ = 0;

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

void DomStatTracker::StopTrackingEvent() {
  DCHECK(is_tracking_event_);
  is_tracking_event_ = false;
}

int DomStatTracker::EventCountHtmlElement() const {
  return event_initial_count_html_element_ + event_count_html_element_created_ -
         event_count_html_element_destroyed_;
}

int DomStatTracker::EventCountHtmlElementDocument() const {
  return event_initial_count_html_element_document_ +
         event_count_html_element_document_added_ -
         event_count_html_element_document_removed_;
}

base::TimeDelta DomStatTracker::GetStopWatchTypeDuration(
    StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool DomStatTracker::IsStopWatchEnabled(int id) const {
  return is_tracking_event_ || id == kStopWatchTypeEventVideoStartDelay;
}

void DomStatTracker::OnStopWatchStopped(int id, base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

}  // namespace dom
}  // namespace cobalt
