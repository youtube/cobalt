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

#include "src/trace_processor/importers/common/event_tracker.h"

<<<<<<< HEAD
#include <cstdint>
#include <optional>

#include "perfetto/base/compiler.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/common/tracks_common.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto::trace_processor {
=======
#include <math.h>
#include <optional>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

EventTracker::EventTracker(TraceProcessorContext* context)
    : context_(context) {}

EventTracker::~EventTracker() = default;

<<<<<<< HEAD
void EventTracker::PushProcessCounterForThread(ProcessCounterForThread pcounter,
                                               int64_t timestamp,
                                               double value,
                                               UniqueTid utid) {
  const auto& counter = context_->storage->counter_table();
  auto opt_id = PushCounter(timestamp, value, kInvalidTrackId);
  if (opt_id) {
    PendingUpidResolutionCounter pending;
    pending.row = counter.FindById(*opt_id)->ToRowNumber().row_number();
    pending.utid = utid;
    pending.counter = pcounter;
    pending_upid_resolution_counter_.emplace_back(pending);
  }
=======
std::optional<CounterId> EventTracker::PushProcessCounterForThread(
    int64_t timestamp,
    double value,
    StringId name_id,
    UniqueTid utid) {
  auto opt_id = PushCounter(timestamp, value, kInvalidTrackId);
  if (opt_id) {
    PendingUpidResolutionCounter pending;
    pending.row = *context_->storage->counter_table().id().IndexOf(*opt_id);
    pending.utid = utid;
    pending.name_id = name_id;
    pending_upid_resolution_counter_.emplace_back(pending);
  }
  return opt_id;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

std::optional<CounterId> EventTracker::PushCounter(int64_t timestamp,
                                                   double value,
                                                   TrackId track_id) {
<<<<<<< HEAD
  auto* counters = context_->storage->mutable_counter_table();
  return counters->Insert({timestamp, track_id, value, {}}).id;
=======
  if (timestamp < max_timestamp_) {
    PERFETTO_DLOG(
        "counter event (ts: %" PRId64 ") out of order by %.4f ms, skipping",
        timestamp, static_cast<double>(max_timestamp_ - timestamp) / 1e6);
    context_->storage->IncrementStats(stats::counter_events_out_of_order);
    return std::nullopt;
  }
  max_timestamp_ = timestamp;

  auto* counter_values = context_->storage->mutable_counter_table();
  return counter_values->Insert({timestamp, track_id, value}).id;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

std::optional<CounterId> EventTracker::PushCounter(
    int64_t timestamp,
    double value,
    TrackId track_id,
<<<<<<< HEAD
    const SetArgsCallback& args_callback) {
=======
    SetArgsCallback args_callback) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  auto maybe_counter_id = PushCounter(timestamp, value, track_id);
  if (maybe_counter_id) {
    auto inserter = context_->args_tracker->AddArgsTo(*maybe_counter_id);
    args_callback(&inserter);
  }
  return maybe_counter_id;
}

void EventTracker::FlushPendingEvents() {
  const auto& thread_table = context_->storage->thread_table();
  for (const auto& pending_counter : pending_upid_resolution_counter_) {
    UniqueTid utid = pending_counter.utid;
<<<<<<< HEAD
    std::optional<UniquePid> upid = thread_table[utid].upid();

    // If we still don't know which process this thread belongs to, fall back
    // onto creating a thread counter track. It's too late to drop data
    // because the counter values have already been inserted.
    TrackId track_id;
    switch (pending_counter.counter.index()) {
      case base::variant_index<ProcessCounterForThread, OomScoreAdj>():
        if (upid.has_value()) {
          track_id = context_->track_tracker->InternTrack(
              tracks::kOomScoreAdjBlueprint, tracks::Dimensions(*upid));
        } else {
          track_id = context_->track_tracker->InternTrack(
              tracks::kOomScoreAdjThreadFallbackBlueprint,
              tracks::Dimensions(utid));
        }
        break;
      case base::variant_index<ProcessCounterForThread, MmEvent>(): {
        const auto& mm_event = std::get<MmEvent>(pending_counter.counter);
        if (upid.has_value()) {
          track_id = context_->track_tracker->InternTrack(
              tracks::kMmEventBlueprint,
              tracks::Dimensions(*upid, mm_event.type, mm_event.metric));
        } else {
          track_id = context_->track_tracker->InternTrack(
              tracks::kMmEventThreadFallbackBlueprint,
              tracks::Dimensions(utid, mm_event.type, mm_event.metric));
        }
        break;
      }
      case base::variant_index<ProcessCounterForThread, RssStat>(): {
        const auto& rss_stat = std::get<RssStat>(pending_counter.counter);
        if (upid.has_value()) {
          track_id = context_->track_tracker->InternTrack(
              tracks::kProcessMemoryBlueprint,
              tracks::Dimensions(*upid, rss_stat.process_memory_key));
        } else {
          track_id = context_->track_tracker->InternTrack(
              tracks::kProcessMemoryThreadFallbackBlueprint,
              tracks::Dimensions(utid, rss_stat.process_memory_key));
        }
        break;
      }
      case base::variant_index<ProcessCounterForThread, JsonCounter>(): {
        const auto& json = std::get<JsonCounter>(pending_counter.counter);
        if (upid.has_value()) {
          track_id = context_->track_tracker->InternTrack(
              tracks::kJsonCounterBlueprint,
              tracks::Dimensions(
                  *upid, context_->storage->GetString(json.counter_name_id)),
              tracks::DynamicName(json.counter_name_id));
        } else {
          track_id = context_->track_tracker->InternTrack(
              tracks::kJsonCounterThreadFallbackBlueprint,
              tracks::Dimensions(
                  utid, context_->storage->GetString(json.counter_name_id)),
              tracks::DynamicName(json.counter_name_id));
        }
        break;
      }
    }
    auto& counter = *context_->storage->mutable_counter_table();
    counter[pending_counter.row].set_track_id(track_id);
=======
    std::optional<UniquePid> upid = thread_table.upid()[utid];

    TrackId track_id = kInvalidTrackId;
    if (upid.has_value()) {
      track_id = context_->track_tracker->InternProcessCounterTrack(
          pending_counter.name_id, *upid);
    } else {
      // If we still don't know which process this thread belongs to, fall back
      // onto creating a thread counter track. It's too late to drop data
      // because the counter values have already been inserted.
      track_id = context_->track_tracker->InternThreadCounterTrack(
          pending_counter.name_id, utid);
    }
    context_->storage->mutable_counter_table()->mutable_track_id()->Set(
        pending_counter.row, track_id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }
  pending_upid_resolution_counter_.clear();
}

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
