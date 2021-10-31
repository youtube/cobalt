// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_BASE_C_VAL_COLLECTION_TIMER_STATS_H_
#define COBALT_BASE_C_VAL_COLLECTION_TIMER_STATS_H_

#include <string>

#include "base/time/time.h"
#include "cobalt/base/c_val_collection_entry_stats.h"

namespace base {

typedef CValDetail::CValCollectionEntryStatsFlushResults<TimeDelta>
    CValCollectionTimerStatsFlushResults;
typedef typename CValCollectionTimerStatsFlushResults::OnFlushCallback
    CValCollectionTimerStatsOnFlushCallback;

// This class is a wrapper around |CValCollectionEntryStats|.
// |CValCollectionTimerStats| calculates the time elapsed between calls to
// Start() and Stop() and adds it as an entry to |CValCollectionEntryStats|. It
// also provides the ability to update the collection's stats and clear its
// entries before its max size is reached via the Flush() call. If no max size
// is provided in the constructor, there is no max size and updates only occur
// when Flush() is called.
template <typename Visibility = CValDebug>
class CValCollectionTimerStats {
 public:
  typedef CValCollectionTimerStatsOnFlushCallback OnFlushCallback;

  explicit CValCollectionTimerStats(const std::string& name)
      : entry_stats_(name) {}
  CValCollectionTimerStats(const std::string& name, size_t max_size,
                           bool enable_entry_list_c_val,
                           const OnFlushCallback& on_flush = OnFlushCallback())
      : entry_stats_(name, max_size, enable_entry_list_c_val, on_flush) {}

  // Start the timer. If the timer is currently running, it is stopped and
  // re-started. If no time is provided, then |base::TimeTicks::Now()| is used.
  void Start() {
    base::TimeTicks now = base::TimeTicks::Now();
    Start(now);
  }
  void Start(const base::TimeTicks& now) {
    Stop(now);
    start_time_ = now;
  }

  // Stop the timer. If it is not already started, then nothing happens. if no
  // time is provided, then |base::TimeTicks::Now()| is used.
  void Stop() {
    base::TimeTicks now = base::TimeTicks::Now();
    Stop(now);
  }
  void Stop(const base::TimeTicks& now) {
    if (start_time_.is_null()) {
      return;
    }

    entry_stats_.AddEntry(now - start_time_);
    start_time_ = base::TimeTicks();
  }

  // Stops any started timer and this sample is ignored.
  void Cancel() { start_time_ = base::TimeTicks(); }

  // Flush the collection. This causes |CValCollectionStats| to update its stats
  // and clear the entries.
  void Flush() { entry_stats_.Flush(); }

 private:
  base::CValCollectionEntryStats<base::TimeDelta, Visibility> entry_stats_;
  base::TimeTicks start_time_;
};

}  // namespace base

#endif  // COBALT_BASE_C_VAL_COLLECTION_TIMER_STATS_H_
