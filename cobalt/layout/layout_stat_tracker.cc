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

#include "cobalt/layout/layout_stat_tracker.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace layout {

LayoutStatTracker::LayoutStatTracker(const std::string& name)
    : total_boxes_(StringPrintf("Count.%s.Layout.Box", name.c_str()), 0,
                   "Total number of layout boxes."),
      layout_dirty_(StringPrintf("Layout.%s.Dirty", name.c_str()), 0,
                   "True if the layout is dirty."),
      boxes_created_count_(0),
      boxes_destroyed_count_(0),
      are_stop_watches_enabled_(false) {
  stop_watch_durations_.resize(kNumStopWatchTypes, base::TimeDelta());
}

LayoutStatTracker::~LayoutStatTracker() {
  FlushPeriodicTracking();

  // Verify that all of the boxes were destroyed.
  DCHECK_EQ(total_boxes_, 0);
}

void LayoutStatTracker::FlushPeriodicTracking() {
  // Update the CVals before clearing the periodic values.
  total_boxes_ += boxes_created_count_ - boxes_destroyed_count_;

  // Now clear the values.
  boxes_created_count_ = 0;
  boxes_destroyed_count_ = 0;

  for (size_t i = 0; i < kNumStopWatchTypes; ++i) {
    stop_watch_durations_[i] = base::TimeDelta();
  }
}

void LayoutStatTracker::OnLayoutDirty() {
  layout_dirty_ = 1;
}

void LayoutStatTracker::OnLayoutClean() {
  layout_dirty_ = 0;
}

void LayoutStatTracker::OnBoxCreated() { ++boxes_created_count_; }

void LayoutStatTracker::OnBoxDestroyed() { ++boxes_destroyed_count_; }

void LayoutStatTracker::EnableStopWatches() {
  are_stop_watches_enabled_ = true;
}

void LayoutStatTracker::DisableStopWatches() {
  are_stop_watches_enabled_ = false;
}

base::TimeDelta LayoutStatTracker::GetStopWatchTypeDuration(
    StopWatchType type) const {
  return stop_watch_durations_[type];
}

bool LayoutStatTracker::IsStopWatchEnabled(int /*id*/) const {
  return are_stop_watches_enabled_;
}

void LayoutStatTracker::OnStopWatchStopped(int id,
                                           base::TimeDelta time_elapsed) {
  stop_watch_durations_[static_cast<size_t>(id)] += time_elapsed;
}

}  // namespace layout
}  // namespace cobalt
