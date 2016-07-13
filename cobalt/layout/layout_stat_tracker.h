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

  // This function updates the CVals from the periodic values and then clears
  // those values.
  void FlushPeriodicTracking();

  // Periodic count-related
  void OnBoxCreated();
  void OnBoxDestroyed();

  int boxes_created_count() const { return boxes_created_count_; }
  int boxes_destroyed_count() const { return boxes_destroyed_count_; }

  // Stop watch-related
  void EnableStopWatches();
  void DisableStopWatches();

  int64 GetStopWatchTypeDuration(StopWatchType type) const;

 private:
  // From base::StopWatchOwner
  bool IsStopWatchEnabled(int id) const OVERRIDE;
  void OnStopWatchStopped(int id, int64 time_elapsed) OVERRIDE;

  // CVals. They are updated when the periodic counts are flushed.
  base::PublicCVal<int> total_boxes_;

  // Periodic counts. The counts are cleared after the CVals are updated in
  // |FlushPeriodicTracking|.
  int boxes_created_count_;
  int boxes_destroyed_count_;

  // Stop watch-related. The durations are cleared after the CVals are updated
  // in |FlushPeriodicTracking|.
  bool are_stop_watches_enabled_;
  std::vector<int64> stop_watch_durations_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LAYOUT_STAT_TRACKER_H_
