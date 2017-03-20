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
    : total_boxes_(StringPrintf("Count.%s.Layout.Box", name.c_str()), 0,
                   "Total number of layout boxes."),
      is_event_active_(false),
      boxes_created_count_(0),
      boxes_destroyed_count_(0),
      update_size_count_(0),
      render_and_animate_count_(0),
      update_cross_references_count_(0) {
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

LayoutStatTracker::~LayoutStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the boxes were destroyed.
  DCHECK_EQ(total_boxes_, 0);
}

void LayoutStatTracker::OnStartEvent() {
  is_event_active_ = true;

  // Flush the periodic tracking prior to starting the event. This ensures that
  // an accurate count of the periodic counts is produced during the event.
  FlushPeriodicTracking();

  // Zero out the stop watch durations so that the numbers will only include the
  // event.
  for (size_t i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watch_durations_[i] = base::TimeDelta();
  }
}

void LayoutStatTracker::OnEndEvent() {
  is_event_active_ = false;

  // Flush the periodic tracking after the event. This updates the cval totals,
  // providing an accurate picture of them at the moment the event ends
  FlushPeriodicTracking();
}

void LayoutStatTracker::OnBoxCreated() { ++boxes_created_count_; }

void LayoutStatTracker::OnBoxDestroyed() { ++boxes_destroyed_count_; }

void LayoutStatTracker::OnUpdateSize() { ++update_size_count_; }

void LayoutStatTracker::OnRenderAndAnimate() { ++render_and_animate_count_; }

void LayoutStatTracker::OnUpdateCrossReferences() {
  ++update_cross_references_count_;
}

base::TimeDelta LayoutStatTracker::GetStopWatchTypeDuration(
    StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool LayoutStatTracker::IsStopWatchEnabled(int /*id*/) const {
  return is_event_active_;
}

void LayoutStatTracker::OnStopWatchStopped(int id,
                                           base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

void LayoutStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  total_boxes_ += boxes_created_count_ - boxes_destroyed_count_;

  // Now clear the values.
  boxes_created_count_ = 0;
  boxes_destroyed_count_ = 0;
  update_size_count_ = 0;
  render_and_animate_count_ = 0;
  update_cross_references_count_ = 0;
}

}  // namespace layout
}  // namespace cobalt
