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

#include "cobalt/dom/dom_stat_tracker.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace dom {

DomStatTracker::DomStatTracker(const std::string& name)
    : total_html_elements_(
          StringPrintf("Count.%s.DOM.HtmlElement", name.c_str()), 0,
          "Total number of HTML elements."),
      html_elements_created_count_(0),
      html_elements_destroyed_count_(0),
      update_matching_rules_count_(0),
      update_computed_style_count_(0),
      are_stop_watches_enabled_(false) {
  stop_watch_durations_.resize(kNumStopWatchTypes, 0);
}

DomStatTracker::~DomStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the elements were destroyed.
  DCHECK_EQ(total_html_elements_, 0);
}

void DomStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  total_html_elements_ +=
      html_elements_created_count_ - html_elements_destroyed_count_;

  // Now clear the values.
  html_elements_created_count_ = 0;
  html_elements_destroyed_count_ = 0;
  update_matching_rules_count_ = 0;
  update_computed_style_count_ = 0;

  for (size_t i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watch_durations_[i] = 0;
  }
}

void DomStatTracker::OnHtmlElementCreated() { ++html_elements_created_count_; }

void DomStatTracker::OnHtmlElementDestroyed() {
  ++html_elements_destroyed_count_;
}

void DomStatTracker::OnUpdateMatchingRules() { ++update_matching_rules_count_; }

void DomStatTracker::OnUpdateComputedStyle() { ++update_computed_style_count_; }

void DomStatTracker::EnableStopWatches() { are_stop_watches_enabled_ = true; }

void DomStatTracker::DisableStopWatches() { are_stop_watches_enabled_ = false; }

int64 DomStatTracker::GetStopWatchTypeDuration(StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool DomStatTracker::IsStopWatchEnabled(int /*id*/) const {
  return are_stop_watches_enabled_;
}

void DomStatTracker::OnStopWatchStopped(int id, int64 time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

}  // namespace dom
}  // namespace cobalt
