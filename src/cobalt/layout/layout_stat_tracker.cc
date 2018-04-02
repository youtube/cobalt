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

#include "cobalt/layout/layout_stat_tracker.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace layout {

LayoutStatTracker::LayoutStatTracker(const std::string& name)
    : count_box_(StringPrintf("Count.%s.Layout.Box", name.c_str()), 0,
                 "Total number of layout boxes."),
      count_box_created_(0),
      count_box_destroyed_(0),
      is_tracking_event_(false),
      event_initial_count_box_(0),
      event_count_box_created_(0),
      event_count_box_destroyed_(0),
      event_count_update_size_(0),
      event_count_render_and_animate_(0),
      event_count_update_cross_references_(0) {
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

LayoutStatTracker::~LayoutStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the boxes were destroyed.
  DCHECK_EQ(count_box_, 0);
}

void LayoutStatTracker::OnBoxCreated() {
  ++count_box_created_;
  if (is_tracking_event_) {
    ++event_count_box_created_;
  }
}

void LayoutStatTracker::OnBoxDestroyed() {
  ++count_box_destroyed_;
  if (is_tracking_event_) {
    ++event_count_box_destroyed_;
  }
}

void LayoutStatTracker::OnUpdateSize() {
  if (is_tracking_event_) {
    ++event_count_update_size_;
  }
}

void LayoutStatTracker::OnRenderAndAnimate() {
  if (is_tracking_event_) {
    ++event_count_render_and_animate_;
  }
}

void LayoutStatTracker::OnUpdateCrossReferences() {
  if (is_tracking_event_) {
    ++event_count_update_cross_references_;
  }
}

void LayoutStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  count_box_ += count_box_created_ - count_box_destroyed_;

  // Now clear the values.
  count_box_created_ = 0;
  count_box_destroyed_ = 0;
}

void LayoutStatTracker::StartTrackingEvent() {
  DCHECK(!is_tracking_event_);
  is_tracking_event_ = true;

  event_initial_count_box_ =
      count_box_ + count_box_created_ - count_box_destroyed_;

  event_count_box_created_ = 0;
  event_count_box_destroyed_ = 0;
  event_count_update_size_ = 0;
  event_count_render_and_animate_ = 0;
  event_count_update_cross_references_ = 0;

  // Zero out the stop watch durations so that the numbers will only include the
  // event.
  for (size_t i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watch_durations_[i] = base::TimeDelta();
  }
}

void LayoutStatTracker::StopTrackingEvent() {
  DCHECK(is_tracking_event_);
  is_tracking_event_ = false;
}

int LayoutStatTracker::EventCountBox() const {
  return event_initial_count_box_ + event_count_box_created_ -
         event_count_box_destroyed_;
}

base::TimeDelta LayoutStatTracker::GetStopWatchTypeDuration(
    StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool LayoutStatTracker::IsStopWatchEnabled(int /*id*/) const {
  return is_tracking_event_;
}

void LayoutStatTracker::OnStopWatchStopped(int id,
                                           base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

}  // namespace layout
}  // namespace cobalt
