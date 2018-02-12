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

#ifndef COBALT_LAYOUT_LAYOUT_STAT_TRACKER_H_
#define COBALT_LAYOUT_LAYOUT_STAT_TRACKER_H_

#include <string>
#include <vector>

#include "cobalt/base/c_val.h"
#include "cobalt/base/stop_watch.h"

namespace cobalt {
namespace layout {

// This class handles tracking layout stats and is intended for single thread
// use.
class LayoutStatTracker : public base::StopWatchOwner {
 public:
  enum StopWatchType {
    kStopWatchTypeLayoutBoxTree,
    kStopWatchTypeBoxGeneration,
    kStopWatchTypeUpdateUsedSizes,
    kStopWatchTypeRenderAndAnimate,
    kNumStopWatchTypes,
  };

  explicit LayoutStatTracker(const std::string& name);
  ~LayoutStatTracker();

  void OnBoxCreated();
  void OnBoxDestroyed();
  void OnUpdateSize();
  void OnRenderAndAnimate();
  void OnUpdateCrossReferences();

  // This function updates the CVals from the periodic values and then clears
  // those values.
  void FlushPeriodicTracking();

  // Event-related
  void StartTrackingEvent();
  void StopTrackingEvent();

  int EventCountBox() const;
  int event_count_box_created() const { return event_count_box_created_; }
  int event_count_box_destroyed() const { return event_count_box_destroyed_; }
  int event_count_update_size() const { return event_count_update_size_; }
  int event_count_render_and_animate() const {
    return event_count_render_and_animate_;
  }
  int event_count_update_cross_references() const {
    return event_count_update_cross_references_;
  }

  base::TimeDelta GetStopWatchTypeDuration(StopWatchType type) const;

 private:
  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const override;
  void OnStopWatchStopped(int id, base::TimeDelta time_elapsed) override;

  // Count cvals that are updated when the periodic tracking is flushed.
  base::CVal<int, base::CValPublic> count_box_;

  // Periodic counts. The counts are cleared after the count cvals are updated
  // in |FlushPeriodicTracking|.
  int count_box_created_;
  int count_box_destroyed_;

  // Event-related
  bool is_tracking_event_;
  int event_initial_count_box_;
  int event_count_box_created_;
  int event_count_box_destroyed_;
  int event_count_update_size_;
  int event_count_render_and_animate_;
  int event_count_update_cross_references_;

  // Stop watch-related.
  std::vector<base::TimeDelta> stop_watch_durations_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_STAT_TRACKER_H_
