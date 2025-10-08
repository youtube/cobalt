/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_

<<<<<<< HEAD
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto::trace_processor {
=======
#include <array>
#include <limits>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessorContext;

// Tracks sched events, instants, and counters.
class EventTracker {
 public:
<<<<<<< HEAD
  struct OomScoreAdj {};
  struct MmEvent {
    const char* type;
    const char* metric;
  };
  struct RssStat {
    const char* process_memory_key;
  };
  struct JsonCounter {
    StringId counter_name_id;
  };
  using ProcessCounterForThread =
      std::variant<OomScoreAdj, MmEvent, RssStat, JsonCounter>;

=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  using SetArgsCallback = std::function<void(ArgsTracker::BoundInserter*)>;

  explicit EventTracker(TraceProcessorContext*);
  EventTracker(const EventTracker&) = delete;
  EventTracker& operator=(const EventTracker&) = delete;
  virtual ~EventTracker();

  // Adds a counter event to the counters table returning the index of the
  // newly added row.
  virtual std::optional<CounterId> PushCounter(int64_t timestamp,
                                               double value,
                                               TrackId track_id);

  // Adds a counter event with args to the counters table returning the index of
  // the newly added row.
  std::optional<CounterId> PushCounter(int64_t timestamp,
                                       double value,
                                       TrackId track_id,
<<<<<<< HEAD
                                       const SetArgsCallback& args_callback);
=======
                                       SetArgsCallback args_callback);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Adds a counter event to the counters table for counter events which
  // should be associated with a process but only have a thread context
  // (e.g. rss_stat events).
  //
  // This function will resolve the utid to a upid when the events are
  // flushed (see |FlushPendingEvents()|).
<<<<<<< HEAD
  void PushProcessCounterForThread(ProcessCounterForThread,
                                   int64_t timestamp,
                                   double value,
                                   UniqueTid utid);
=======
  virtual std::optional<CounterId> PushProcessCounterForThread(
      int64_t timestamp,
      double value,
      StringId name_id,
      UniqueTid utid);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Called at the end of trace to flush any events which are pending to the
  // storage.
  void FlushPendingEvents();

<<<<<<< HEAD
 private:
  // Represents a counter event which is currently pending upid resolution.
  struct PendingUpidResolutionCounter {
    ProcessCounterForThread counter;
    uint32_t row = 0;
=======
  // For SchedEventTracker.
  int64_t max_timestamp() const { return max_timestamp_; }
  void UpdateMaxTimestamp(int64_t ts) {
    max_timestamp_ = std::max(ts, max_timestamp_);
  }

 private:
  // Represents a counter event which is currently pending upid resolution.
  struct PendingUpidResolutionCounter {
    uint32_t row = 0;
    StringId name_id = kNullStringId;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    UniqueTid utid = 0;
  };

  // Store the rows in the counters table which need upids resolved.
  std::vector<PendingUpidResolutionCounter> pending_upid_resolution_counter_;

<<<<<<< HEAD
  TraceProcessorContext* const context_;
};
}  // namespace perfetto::trace_processor
=======
  // Timestamp of the previous event. Used to discard events arriving out
  // of order.
  int64_t max_timestamp_ = 0;

  TraceProcessorContext* const context_;
};
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_EVENT_TRACKER_H_
