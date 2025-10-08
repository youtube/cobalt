/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/track_event_tracker.h"

<<<<<<< HEAD
#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/process_track_translation_table.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/common/tracks_common.h"
#include "src/trace_processor/importers/common/tracks_internal.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto::trace_processor {

namespace {

constexpr auto kThreadCounterTrackBlueprint = tracks::CounterBlueprint(
    "thread_counter_track_event",
    tracks::DynamicUnitBlueprint(),
    tracks::DimensionBlueprints(tracks::kThreadDimensionBlueprint,
                                tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

constexpr auto kProcessCounterTrackBlueprint = tracks::CounterBlueprint(
    "process_counter_track_event",
    tracks::DynamicUnitBlueprint(),
    tracks::DimensionBlueprints(tracks::kProcessDimensionBlueprint,
                                tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

constexpr auto kGlobalCounterTrackBlueprint = tracks::CounterBlueprint(
    "global_counter_track_event",
    tracks::DynamicUnitBlueprint(),
    tracks::DimensionBlueprints(tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

constexpr auto kThreadTrackBlueprint = tracks::SliceBlueprint(
    "thread_track_event",
    tracks::DimensionBlueprints(tracks::kThreadDimensionBlueprint,
                                tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

constexpr auto kProcessTrackBlueprint = tracks::SliceBlueprint(
    "process_track_event",
    tracks::DimensionBlueprints(tracks::kProcessDimensionBlueprint,
                                tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

constexpr auto kGlobalTrackBlueprint = tracks::SliceBlueprint(
    "global_track_event",
    tracks::DimensionBlueprints(tracks::LongDimensionBlueprint("track_uuid")),
    tracks::DynamicNameBlueprint());

}  // namespace

TrackEventTracker::TrackEventTracker(TraceProcessorContext* context)
    : source_key_(context->storage->InternString("source")),
      source_id_key_(context->storage->InternString("trace_id")),
      is_root_in_scope_key_(context->storage->InternString("is_root_in_scope")),
      category_key_(context->storage->InternString("category")),
      builtin_counter_type_key_(
          context->storage->InternString("builtin_counter_type")),
      has_first_packet_on_sequence_key_id_(
          context->storage->InternString("has_first_packet_on_sequence")),
      child_ordering_key_(context->storage->InternString("child_ordering")),
      explicit_id_(context->storage->InternString("explicit")),
      lexicographic_id_(context->storage->InternString("lexicographic")),
      chronological_id_(context->storage->InternString("chronological")),
      sibling_order_rank_key_(
          context->storage->InternString("sibling_order_rank")),
=======
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/args_translation_table.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"

namespace perfetto {
namespace trace_processor {

TrackEventTracker::TrackEventTracker(TraceProcessorContext* context)
    : source_key_(context->storage->InternString("source")),
      source_id_key_(context->storage->InternString("source_id")),
      is_root_in_scope_key_(context->storage->InternString("is_root_in_scope")),
      category_key_(context->storage->InternString("category")),
      has_first_packet_on_sequence_key_id_(
          context->storage->InternString("has_first_packet_on_sequence")),
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      descriptor_source_(context->storage->InternString("descriptor")),
      default_descriptor_track_name_(
          context->storage->InternString("Default Track")),
      context_(context) {}

<<<<<<< HEAD
void TrackEventTracker::ReserveDescriptorTrack(
    uint64_t uuid,
    const DescriptorTrackReservation& reservation) {
  if (uuid == kDefaultDescriptorTrackUuid && reservation.parent_uuid) {
    PERFETTO_DLOG(
        "Default track (uuid 0) cannot have a parent uui specified. Ignoring "
        "the descriptor.");
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return;
  }

  auto [it, inserted] = reserved_descriptor_tracks_.Insert(uuid, reservation);
  if (inserted) {
    return;
  }

  if (!it->IsForSameTrack(reservation)) {
    PERFETTO_DLOG("New track reservation for track with uuid %" PRIu64
=======
void TrackEventTracker::ReserveDescriptorProcessTrack(uint64_t uuid,
                                                      StringId name,
                                                      uint32_t pid,
                                                      int64_t timestamp) {
  DescriptorTrackReservation reservation;
  reservation.min_timestamp = timestamp;
  reservation.pid = pid;
  reservation.name = name;

  std::map<uint64_t, DescriptorTrackReservation>::iterator it;
  bool inserted;
  std::tie(it, inserted) =
      reserved_descriptor_tracks_.insert(std::make_pair<>(uuid, reservation));

  if (inserted)
    return;

  if (!it->second.IsForSameTrack(reservation)) {
    // Process tracks should not be reassigned to a different pid later (neither
    // should the type of the track change).
    PERFETTO_DLOG("New track reservation for process track with uuid %" PRIu64
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                  " doesn't match earlier one",
                  uuid);
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return;
  }
<<<<<<< HEAD
  it->min_timestamp = std::min(it->min_timestamp, reservation.min_timestamp);
}

std::optional<TrackEventTracker::ResolvedDescriptorTrack>
TrackEventTracker::GetDescriptorTrackImpl(
    uint64_t uuid,
    StringId event_name,
    std::optional<uint32_t> packet_sequence_id) {
  auto* resolved_ptr = resolved_descriptor_tracks_.Find(uuid);
  if (resolved_ptr) {
    if (event_name.is_null()) {
      return *resolved_ptr;
    }

    // Update the name to match the first non-null and valid event name. We need
    // this because TrackEventParser calls |GetDescriptorTrack| with
    // kNullStringId which means we cannot just have the code below for updating
    // the name
    DescriptorTrackReservation* reservation_ptr =
        reserved_descriptor_tracks_.Find(uuid);
    PERFETTO_CHECK(reservation_ptr);
    auto* tracks = context_->storage->mutable_track_table();
    auto rr = *tracks->FindById(resolved_ptr->track_id());
    bool is_root_thread_process_or_counter = reservation_ptr->pid ||
                                             reservation_ptr->tid ||
                                             reservation_ptr->is_counter;
    if (rr.name().is_null() && !is_root_thread_process_or_counter) {
      if (resolved_ptr->scope() == ResolvedDescriptorTrack::Scope::kProcess) {
        rr.set_name(context_->process_track_translation_table->TranslateName(
            event_name));
      } else {
        rr.set_name(event_name);
      }
    }
    return *resolved_ptr;
  }

  DescriptorTrackReservation* reservation_ptr =
      reserved_descriptor_tracks_.Find(uuid);
  if (!reservation_ptr) {
    if (uuid != kDefaultDescriptorTrackUuid) {
      return std::nullopt;
    }

    // For the default track, if there's no reservation, forcefully create it
    // as it's always allowed to emit events on it, even without emitting a
    // descriptor.
    DescriptorTrackReservation r;
    r.parent_uuid = 0;
    r.name = default_descriptor_track_name_;
    ReserveDescriptorTrack(kDefaultDescriptorTrackUuid, r);

    reservation_ptr = reserved_descriptor_tracks_.Find(uuid);
    PERFETTO_CHECK(reservation_ptr);
  }

  // Before trying to resolve anything, ensure that the hierarchy of tracks is
  // well defined.
  if (!IsTrackHierarchyValid(uuid)) {
    return std::nullopt;
  }

  // Resolve process and thread id for tracks produced from within a pid
  // namespace.
  //
  // Get the root-level trusted_pid for the process that produces the track
  // event.
  std::optional<uint32_t> trusted_pid =
      context_->process_tracker->GetTrustedPid(uuid);
  DescriptorTrackReservation& reservation = *reservation_ptr;

  // Try to resolve to root-level pid and tid if the process is pid-namespaced.
  if (trusted_pid && reservation.tid) {
    std::optional<uint32_t> resolved_tid =
        context_->process_tracker->ResolveNamespacedTid(*trusted_pid,
                                                        *reservation.tid);
    if (resolved_tid) {
      reservation.tid = *resolved_tid;
    }
  }
  if (trusted_pid && reservation.pid) {
    std::optional<uint32_t> resolved_pid =
        context_->process_tracker->ResolveNamespacedTid(*trusted_pid,
                                                        *reservation.pid);
    if (resolved_pid) {
      reservation.pid = *resolved_pid;
    }
  }

  bool is_root_thread_process_or_counter = reservation_ptr->pid ||
                                           reservation_ptr->tid ||
                                           reservation_ptr->is_counter;
  if (reservation.name.is_null() && !is_root_thread_process_or_counter) {
    reservation.name = event_name;
  }

  // If the reservation does not have a name specified, name it the same
  // as the first event on the track. Note this only applies for non-root and
  // non-counter tracks.
  auto [it, inserted] = resolved_descriptor_tracks_.Insert(
      uuid, ResolveDescriptorTrack(uuid, reservation, packet_sequence_id));
  PERFETTO_CHECK(inserted);
  return *it;
}

TrackEventTracker::ResolvedDescriptorTrack
TrackEventTracker::ResolveDescriptorTrack(
    uint64_t uuid,
    const DescriptorTrackReservation& reservation,
    std::optional<uint32_t> packet_sequence_id) {
  TrackTracker::SetArgsCallback args_fn_root =
      [&, this](ArgsTracker::BoundInserter& inserter) {
        AddTrackArgs(uuid, packet_sequence_id, reservation, true /* is_root*/,
                     inserter);
      };
  TrackTracker::SetArgsCallback args_fn_non_root =
      [&, this](ArgsTracker::BoundInserter& inserter) {
        AddTrackArgs(uuid, packet_sequence_id, reservation, false /* is_root*/,
                     inserter);
      };

  // Try to resolve any parent tracks recursively, too.
  std::optional<ResolvedDescriptorTrack> parent_resolved_track;
  if (reservation.parent_uuid != kDefaultDescriptorTrackUuid) {
    parent_resolved_track = GetDescriptorTrackImpl(
        reservation.parent_uuid, kNullStringId, packet_sequence_id);
=======

  it->second.min_timestamp = std::min(it->second.min_timestamp, timestamp);
}

void TrackEventTracker::ReserveDescriptorThreadTrack(uint64_t uuid,
                                                     uint64_t parent_uuid,
                                                     StringId name,
                                                     uint32_t pid,
                                                     uint32_t tid,
                                                     int64_t timestamp,
                                                     bool use_separate_track) {
  DescriptorTrackReservation reservation;
  reservation.min_timestamp = timestamp;
  reservation.parent_uuid = parent_uuid;
  reservation.pid = pid;
  reservation.tid = tid;
  reservation.name = name;
  reservation.use_separate_track = use_separate_track;

  std::map<uint64_t, DescriptorTrackReservation>::iterator it;
  bool inserted;
  std::tie(it, inserted) =
      reserved_descriptor_tracks_.insert(std::make_pair<>(uuid, reservation));

  if (inserted)
    return;

  if (!it->second.IsForSameTrack(reservation)) {
    // Thread tracks should not be reassigned to a different pid/tid later
    // (neither should the type of the track change).
    PERFETTO_DLOG("New track reservation for thread track with uuid %" PRIu64
                  " doesn't match earlier one",
                  uuid);
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return;
  }

  it->second.min_timestamp = std::min(it->second.min_timestamp, timestamp);
}

void TrackEventTracker::ReserveDescriptorCounterTrack(
    uint64_t uuid,
    uint64_t parent_uuid,
    StringId name,
    StringId category,
    int64_t unit_multiplier,
    bool is_incremental,
    uint32_t packet_sequence_id) {
  DescriptorTrackReservation reservation;
  reservation.parent_uuid = parent_uuid;
  reservation.is_counter = true;
  reservation.name = name;
  reservation.category = category;
  reservation.unit_multiplier = unit_multiplier;
  reservation.is_incremental = is_incremental;
  // Incrementally encoded counters are only valid on a single sequence.
  if (is_incremental)
    reservation.packet_sequence_id = packet_sequence_id;

  std::map<uint64_t, DescriptorTrackReservation>::iterator it;
  bool inserted;
  std::tie(it, inserted) =
      reserved_descriptor_tracks_.insert(std::make_pair<>(uuid, reservation));

  if (inserted || it->second.IsForSameTrack(reservation))
    return;

  // Counter tracks should not be reassigned to a different parent track later
  // (neither should the type of the track change).
  PERFETTO_DLOG("New track reservation for counter track with uuid %" PRIu64
                " doesn't match earlier one",
                uuid);
  context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
}

void TrackEventTracker::ReserveDescriptorChildTrack(uint64_t uuid,
                                                    uint64_t parent_uuid,
                                                    StringId name) {
  DescriptorTrackReservation reservation;
  reservation.parent_uuid = parent_uuid;
  reservation.name = name;

  std::map<uint64_t, DescriptorTrackReservation>::iterator it;
  bool inserted;
  std::tie(it, inserted) =
      reserved_descriptor_tracks_.insert(std::make_pair<>(uuid, reservation));

  if (inserted || it->second.IsForSameTrack(reservation))
    return;

  // Child tracks should not be reassigned to a different parent track later
  // (neither should the type of the track change).
  PERFETTO_DLOG("New track reservation for child track with uuid %" PRIu64
                " doesn't match earlier one",
                uuid);
  context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
}

TrackId TrackEventTracker::InsertThreadTrack(UniqueTid utid) {
  tables::ThreadTrackTable::Row row;
  row.utid = utid;
  auto* thread_tracks = context_->storage->mutable_thread_track_table();
  return thread_tracks->Insert(row).id;
}

TrackId TrackEventTracker::InternThreadTrack(UniqueTid utid) {
  auto it = thread_tracks_.find(utid);
  if (it != thread_tracks_.end()) {
    return it->second;
  }
  return thread_tracks_[utid] = InsertThreadTrack(utid);
}

std::optional<TrackId> TrackEventTracker::GetDescriptorTrack(
    uint64_t uuid,
    StringId event_name,
    std::optional<uint32_t> packet_sequence_id) {
  std::optional<TrackId> track_id =
      GetDescriptorTrackImpl(uuid, packet_sequence_id);
  if (!track_id || event_name.is_null())
    return track_id;

  // Update the name of the track if unset and the track is not the primary
  // track of a process/thread or a counter track.
  auto* tracks = context_->storage->mutable_track_table();
  uint32_t row = *tracks->id().IndexOf(*track_id);
  if (!tracks->name()[row].is_null())
    return track_id;

  // Check reservation for track type.
  auto reservation_it = reserved_descriptor_tracks_.find(uuid);
  PERFETTO_CHECK(reservation_it != reserved_descriptor_tracks_.end());

  if (reservation_it->second.pid || reservation_it->second.tid ||
      reservation_it->second.is_counter) {
    return track_id;
  }
  tracks->mutable_name()->Set(row, event_name);
  return track_id;
}

std::optional<TrackId> TrackEventTracker::GetDescriptorTrackImpl(
    uint64_t uuid,
    std::optional<uint32_t> packet_sequence_id) {
  auto it = descriptor_tracks_.find(uuid);
  if (it != descriptor_tracks_.end())
    return it->second;

  std::optional<ResolvedDescriptorTrack> resolved_track =
      ResolveDescriptorTrack(uuid, nullptr);
  if (!resolved_track)
    return std::nullopt;

  // The reservation must exist as |resolved_track| would have been std::nullopt
  // otherwise.
  auto reserved_it = reserved_descriptor_tracks_.find(uuid);
  PERFETTO_CHECK(reserved_it != reserved_descriptor_tracks_.end());

  const auto& reservation = reserved_it->second;

  // We resolve parent_id here to ensure that it's going to be smaller
  // than the id of the child.
  std::optional<TrackId> parent_id;
  if (reservation.parent_uuid != 0) {
    parent_id = GetDescriptorTrackImpl(reservation.parent_uuid);
  }

  TrackId track_id = CreateTrackFromResolved(*resolved_track);
  descriptor_tracks_[uuid] = track_id;

  auto args = context_->args_tracker->AddArgsTo(track_id);
  args.AddArg(source_key_, Variadic::String(descriptor_source_))
      .AddArg(source_id_key_, Variadic::Integer(static_cast<int64_t>(uuid)))
      .AddArg(is_root_in_scope_key_,
              Variadic::Boolean(resolved_track->is_root_in_scope()));
  if (!reservation.category.is_null())
    args.AddArg(category_key_, Variadic::String(reservation.category));
  if (packet_sequence_id &&
      sequences_with_first_packet_.find(*packet_sequence_id) !=
          sequences_with_first_packet_.end()) {
    args.AddArg(has_first_packet_on_sequence_key_id_, Variadic::Boolean(true));
  }

  auto* tracks = context_->storage->mutable_track_table();
  auto row_ref = *tracks->FindById(track_id);
  if (parent_id) {
    row_ref.set_parent_id(*parent_id);
  }

  if (reservation.name.is_null())
    return track_id;

  // Initialize the track name here, so that, if a name was given in the
  // reservation, it is set immediately after resolution takes place.
  row_ref.set_name(reservation.name);
  return track_id;
}

TrackId TrackEventTracker::CreateTrackFromResolved(
    const ResolvedDescriptorTrack& track) {
  if (track.is_root_in_scope()) {
    switch (track.scope()) {
      case ResolvedDescriptorTrack::Scope::kThread: {
        if (track.use_separate_track()) {
          return InternThreadTrack(track.utid());
        }
        return context_->track_tracker->InternThreadTrack(track.utid());
      }
      case ResolvedDescriptorTrack::Scope::kProcess:
        return context_->track_tracker->InternProcessTrack(track.upid());
      case ResolvedDescriptorTrack::Scope::kGlobal:
        // Will be handled below.
        break;
    }
  }

  switch (track.scope()) {
    case ResolvedDescriptorTrack::Scope::kThread: {
      if (track.is_counter()) {
        tables::ThreadCounterTrackTable::Row row;
        row.utid = track.utid();

        auto* thread_counter_tracks =
            context_->storage->mutable_thread_counter_track_table();
        return thread_counter_tracks->Insert(row).id;
      }

      return InsertThreadTrack(track.utid());
    }
    case ResolvedDescriptorTrack::Scope::kProcess: {
      if (track.is_counter()) {
        tables::ProcessCounterTrackTable::Row row;
        row.upid = track.upid();

        auto* process_counter_tracks =
            context_->storage->mutable_process_counter_track_table();
        return process_counter_tracks->Insert(row).id;
      }

      tables::ProcessTrackTable::Row row;
      row.upid = track.upid();

      auto* process_tracks = context_->storage->mutable_process_track_table();
      return process_tracks->Insert(row).id;
    }
    case ResolvedDescriptorTrack::Scope::kGlobal: {
      if (track.is_counter())
        return context_->storage->mutable_counter_track_table()->Insert({}).id;
      return context_->storage->mutable_track_table()->Insert({}).id;
    }
  }
  PERFETTO_FATAL("For GCC");
}

std::optional<TrackEventTracker::ResolvedDescriptorTrack>
TrackEventTracker::ResolveDescriptorTrack(
    uint64_t uuid,
    std::vector<uint64_t>* descendent_uuids) {
  auto it = resolved_descriptor_tracks_.find(uuid);
  if (it != resolved_descriptor_tracks_.end())
    return it->second;

  auto reservation_it = reserved_descriptor_tracks_.find(uuid);
  if (reservation_it == reserved_descriptor_tracks_.end())
    return std::nullopt;

  // Resolve process and thread id for tracks produced from within a pid
  // namespace.
  // Get the root-level trusted_pid for the process that produces the track
  // event.
  auto opt_trusted_pid = context_->process_tracker->GetTrustedPid(uuid);
  auto& reservation = reservation_it->second;
  // Try to resolve to root-level pid and tid if the process is pid-namespaced.
  if (opt_trusted_pid && reservation.tid) {
    auto opt_resolved_tid = context_->process_tracker->ResolveNamespacedTid(
        *opt_trusted_pid, *reservation.tid);
    if (opt_resolved_tid)
      reservation.tid = *opt_resolved_tid;
  }
  if (opt_trusted_pid && reservation.pid) {
    auto opt_resolved_pid = context_->process_tracker->ResolveNamespacedTid(
        *opt_trusted_pid, *reservation.pid);
    if (opt_resolved_pid)
      reservation.pid = *opt_resolved_pid;
  }

  std::optional<ResolvedDescriptorTrack> resolved_track =
      ResolveDescriptorTrackImpl(uuid, reservation, descendent_uuids);
  if (!resolved_track) {
    return std::nullopt;
  }
  resolved_descriptor_tracks_[uuid] = *resolved_track;
  return resolved_track;
}

std::optional<TrackEventTracker::ResolvedDescriptorTrack>
TrackEventTracker::ResolveDescriptorTrackImpl(
    uint64_t uuid,
    const DescriptorTrackReservation& reservation,
    std::vector<uint64_t>* descendent_uuids) {
  static constexpr size_t kMaxAncestors = 10;

  // Try to resolve any parent tracks recursively, too.
  std::optional<ResolvedDescriptorTrack> parent_resolved_track;
  if (reservation.parent_uuid) {
    // Input data may contain loops or extremely long ancestor track chains. To
    // avoid stack overflow in these situations, we keep track of the ancestors
    // seen in the recursion.
    std::unique_ptr<std::vector<uint64_t>> owned_descendent_uuids;
    if (!descendent_uuids) {
      owned_descendent_uuids.reset(new std::vector<uint64_t>());
      descendent_uuids = owned_descendent_uuids.get();
    }
    descendent_uuids->push_back(uuid);

    if (descendent_uuids->size() > kMaxAncestors) {
      PERFETTO_ELOG(
          "Too many ancestors in parent_track_uuid hierarchy at track %" PRIu64
          " with parent %" PRIu64,
          uuid, reservation.parent_uuid);
      return std::nullopt;
    }

    if (std::find(descendent_uuids->begin(), descendent_uuids->end(),
                  reservation.parent_uuid) != descendent_uuids->end()) {
      PERFETTO_ELOG(
          "Loop detected in parent_track_uuid hierarchy at track %" PRIu64
          " with parent %" PRIu64,
          uuid, reservation.parent_uuid);
      return std::nullopt;
    }

    parent_resolved_track =
        ResolveDescriptorTrack(reservation.parent_uuid, descendent_uuids);
    if (!parent_resolved_track) {
      PERFETTO_ELOG("Unknown parent track %" PRIu64 " for track %" PRIu64,
                    reservation.parent_uuid, uuid);
    }

    descendent_uuids->pop_back();
    if (owned_descendent_uuids)
      descendent_uuids = nullptr;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  if (reservation.tid) {
    UniqueTid utid = context_->process_tracker->UpdateThread(*reservation.tid,
                                                             *reservation.pid);
<<<<<<< HEAD
    auto [it, inserted] = descriptor_uuids_by_utid_.Insert(utid, uuid);
    if (!inserted) {
      // We already saw a another track with a different uuid for this thread.
      // Since there should only be one descriptor track for each thread, we
      // assume that its tid was reused. So, start a new thread.
      uint64_t old_uuid = *it;
      PERFETTO_DCHECK(old_uuid != uuid);  // Every track is only resolved once.
      *it = uuid;
=======
    auto it_and_inserted =
        descriptor_uuids_by_utid_.insert(std::make_pair<>(utid, uuid));
    if (!it_and_inserted.second) {
      // We already saw a another track with a different uuid for this thread.
      // Since there should only be one descriptor track for each thread, we
      // assume that its tid was reused. So, start a new thread.
      uint64_t old_uuid = it_and_inserted.first->second;
      PERFETTO_DCHECK(old_uuid != uuid);  // Every track is only resolved once.
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

      PERFETTO_DLOG("Detected tid reuse (pid: %" PRIu32 " tid: %" PRIu32
                    ") from track descriptors (old uuid: %" PRIu64
                    " new uuid: %" PRIu64 " timestamp: %" PRId64 ")",
                    *reservation.pid, *reservation.tid, old_uuid, uuid,
                    reservation.min_timestamp);

<<<<<<< HEAD
      // Associate the new thread with its process.
      utid = context_->process_tracker->StartNewThread(std::nullopt,
                                                       *reservation.tid);
      UniqueTid updated_utid = context_->process_tracker->UpdateThread(
          *reservation.tid, *reservation.pid);
      PERFETTO_CHECK(updated_utid == utid);
    }

    TrackId id;
    if (reservation.is_counter) {
      id = context_->track_tracker->InternTrack(
          kThreadCounterTrackBlueprint,
          tracks::Dimensions(utid, static_cast<int64_t>(uuid)),
          tracks::DynamicName(reservation.name), args_fn_root,
          tracks::DynamicUnit(reservation.counter_details->unit));
    } else if (reservation.use_separate_track) {
      id = context_->track_tracker->InternTrack(
          kThreadTrackBlueprint,
          tracks::Dimensions(utid, static_cast<int64_t>(uuid)),
          tracks::DynamicName(reservation.name), args_fn_root);
    } else {
      id = context_->track_tracker->InternThreadTrack(utid);
    }
    return ResolvedDescriptorTrack::Thread(id, utid, reservation.is_counter,
                                           true /* is_root*/);
=======
      utid = context_->process_tracker->StartNewThread(std::nullopt,
                                                       *reservation.tid);

      // Associate the new thread with its process.
      PERFETTO_CHECK(context_->process_tracker->UpdateThread(
                         *reservation.tid, *reservation.pid) == utid);

      descriptor_uuids_by_utid_[utid] = uuid;
    }
    return ResolvedDescriptorTrack::Thread(utid, false /* is_counter */,
                                           true /* is_root*/,
                                           reservation.use_separate_track);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  if (reservation.pid) {
    UniquePid upid =
        context_->process_tracker->GetOrCreateProcess(*reservation.pid);
<<<<<<< HEAD
    auto [it, inserted] = descriptor_uuids_by_upid_.Insert(upid, uuid);
    if (!inserted) {
      // We already saw a another track with a different uuid for this process.
      // Since there should only be one descriptor track for each process,
      // we assume that its pid was reused. So, start a new process.
      uint64_t old_uuid = *it;
      PERFETTO_DCHECK(old_uuid != uuid);  // Every track is only resolved once.
      *it = uuid;
=======
    auto it_and_inserted =
        descriptor_uuids_by_upid_.insert(std::make_pair<>(upid, uuid));
    if (!it_and_inserted.second) {
      // We already saw a another track with a different uuid for this process.
      // Since there should only be one descriptor track for each process, we
      // assume that its pid was reused. So, start a new process.
      uint64_t old_uuid = it_and_inserted.first->second;
      PERFETTO_DCHECK(old_uuid != uuid);  // Every track is only resolved once.
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

      PERFETTO_DLOG("Detected pid reuse (pid: %" PRIu32
                    ") from track descriptors (old uuid: %" PRIu64
                    " new uuid: %" PRIu64 " timestamp: %" PRId64 ")",
                    *reservation.pid, old_uuid, uuid,
                    reservation.min_timestamp);

      upid = context_->process_tracker->StartNewProcess(
          std::nullopt, std::nullopt, *reservation.pid, kNullStringId,
          ThreadNamePriority::kTrackDescriptor);
<<<<<<< HEAD
    }
    StringId translated_name =
        context_->process_track_translation_table->TranslateName(
            reservation.name);
    TrackId id;
    if (reservation.is_counter) {
      id = context_->track_tracker->InternTrack(
          kProcessCounterTrackBlueprint,
          tracks::Dimensions(upid, static_cast<int64_t>(uuid)),
          tracks::DynamicName(translated_name), args_fn_root,
          tracks::DynamicUnit(reservation.counter_details->unit));
    } else {
      id = context_->track_tracker->InternTrack(
          kProcessTrackBlueprint,
          tracks::Dimensions(upid, static_cast<int64_t>(uuid)),
          tracks::DynamicName(translated_name), args_fn_root);
    }
    return ResolvedDescriptorTrack::Process(id, upid, reservation.is_counter,
                                            true /* is_root*/);
  }

  auto set_parent_id = [&](TrackId id) {
    if (parent_resolved_track) {
      auto rr = context_->storage->mutable_track_table()->FindById(id);
      PERFETTO_CHECK(rr);
      rr->set_parent_id(parent_resolved_track->track_id());
    }
  };

  if (parent_resolved_track) {
    switch (parent_resolved_track->scope()) {
      case ResolvedDescriptorTrack::Scope::kThread: {
        // If parent is a thread track, create another thread-associated track.
        TrackId id;
        if (reservation.is_counter) {
          id = context_->track_tracker->InternTrack(
              kThreadCounterTrackBlueprint,
              tracks::Dimensions(parent_resolved_track->utid(),
                                 static_cast<int64_t>(uuid)),
              tracks::DynamicName(reservation.name), args_fn_non_root,
              tracks::DynamicUnit(reservation.counter_details->unit));
        } else {
          id = context_->track_tracker->InternTrack(
              kThreadTrackBlueprint,
              tracks::Dimensions(parent_resolved_track->utid(),
                                 static_cast<int64_t>(uuid)),
              tracks::DynamicName(reservation.name), args_fn_non_root);
        }
        // If the parent has a process descriptor set, promote this track
        // to also be a root thread level track. This is necessary for
        // backcompat reasons: see the comment on parent_uuid in
        // TrackDescriptor.
        if (!parent_resolved_track->is_root()) {
          set_parent_id(id);
        }
        return ResolvedDescriptorTrack::Thread(
            id, parent_resolved_track->utid(), reservation.is_counter,
            false /* is_root*/);
      }
      case ResolvedDescriptorTrack::Scope::kProcess: {
        // If parent is a process track, create another process-associated
        // track.
        StringId translated_name =
            context_->process_track_translation_table->TranslateName(
                reservation.name);
        TrackId id;
        if (reservation.is_counter) {
          id = context_->track_tracker->InternTrack(
              kProcessCounterTrackBlueprint,
              tracks::Dimensions(parent_resolved_track->upid(),
                                 static_cast<int64_t>(uuid)),
              tracks::DynamicName(translated_name), args_fn_non_root,
              tracks::DynamicUnit(reservation.counter_details->unit));
        } else {
          id = context_->track_tracker->InternTrack(
              kProcessTrackBlueprint,
              tracks::Dimensions(parent_resolved_track->upid(),
                                 static_cast<int64_t>(uuid)),
              tracks::DynamicName(translated_name), args_fn_non_root);
        }
        // If the parent has a thread descriptor set, promote this track
        // to also be a root thread level track. This is necessary for
        // backcompat reasons: see the comment on parent_uuid in
        // TrackDescriptor.
        if (!parent_resolved_track->is_root()) {
          set_parent_id(id);
        }
        return ResolvedDescriptorTrack::Process(
            id, parent_resolved_track->upid(), reservation.is_counter,
            false /* is_root*/);
      }
=======

      descriptor_uuids_by_upid_[upid] = uuid;
    }
    return ResolvedDescriptorTrack::Process(upid, false /* is_counter */,
                                            true /* is_root*/);
  }

  if (parent_resolved_track) {
    switch (parent_resolved_track->scope()) {
      case ResolvedDescriptorTrack::Scope::kThread:
        // If parent is a thread track, create another thread-associated track.
        return ResolvedDescriptorTrack::Thread(
            parent_resolved_track->utid(), reservation.is_counter,
            false /* is_root*/, parent_resolved_track->use_separate_track());
      case ResolvedDescriptorTrack::Scope::kProcess:
        // If parent is a process track, create another process-associated
        // track.
        return ResolvedDescriptorTrack::Process(parent_resolved_track->upid(),
                                                reservation.is_counter,
                                                false /* is_root*/);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      case ResolvedDescriptorTrack::Scope::kGlobal:
        break;
    }
  }

<<<<<<< HEAD
  // root_in_scope only matters for legacy JSON export. This is somewhat related
  // but intentionally distinct from our handling of parent_id relationships.
  bool is_root_in_scope = uuid == kDefaultDescriptorTrackUuid;
  TrackId id;
  if (reservation.is_counter) {
    id = context_->track_tracker->InternTrack(
        kGlobalCounterTrackBlueprint,
        tracks::Dimensions(static_cast<int64_t>(uuid)),
        tracks::DynamicName(reservation.name),
        is_root_in_scope ? args_fn_root : args_fn_non_root,
        tracks::DynamicUnit(reservation.counter_details->unit));
  } else {
    id = context_->track_tracker->InternTrack(
        kGlobalTrackBlueprint, tracks::Dimensions(static_cast<int64_t>(uuid)),
        tracks::DynamicName(reservation.name),
        is_root_in_scope ? args_fn_root : args_fn_non_root);
  }
  set_parent_id(id);
  return ResolvedDescriptorTrack::Global(id, reservation.is_counter);
=======
  // Otherwise create a global track.

  // The global track with no uuid is the default global track (e.g. for
  // global instant events). Any other global tracks are considered children
  // of the default track.
  bool is_root_in_scope = !parent_resolved_track;
  if (!parent_resolved_track && uuid) {
    // Detect loops where the default track has a parent that itself is a
    // global track (and thus should be parent of the default track).
    if (descendent_uuids &&
        std::find(descendent_uuids->begin(), descendent_uuids->end(),
                  kDefaultDescriptorTrackUuid) != descendent_uuids->end()) {
      PERFETTO_ELOG(
          "Loop detected in parent_track_uuid hierarchy at track %" PRIu64
          " with parent %" PRIu64,
          uuid, kDefaultDescriptorTrackUuid);
      return std::nullopt;
    }

    // This track will be implicitly a child of the default global track.
    is_root_in_scope = false;
  }
  return ResolvedDescriptorTrack::Global(reservation.is_counter,
                                         is_root_in_scope);
}

TrackId TrackEventTracker::GetOrCreateDefaultDescriptorTrack() {
  // If the default track was already reserved (e.g. because a producer emitted
  // a descriptor for it) or created, resolve and return it.
  std::optional<TrackId> track_id =
      GetDescriptorTrack(kDefaultDescriptorTrackUuid);
  if (track_id)
    return *track_id;

  // Otherwise reserve a new track and resolve it.
  ReserveDescriptorChildTrack(kDefaultDescriptorTrackUuid, /*parent_uuid=*/0,
                              default_descriptor_track_name_);
  return *GetDescriptorTrack(kDefaultDescriptorTrackUuid);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

std::optional<double> TrackEventTracker::ConvertToAbsoluteCounterValue(
    uint64_t counter_track_uuid,
    uint32_t packet_sequence_id,
    double value) {
<<<<<<< HEAD
  auto* reservation_ptr = reserved_descriptor_tracks_.Find(counter_track_uuid);
  if (!reservation_ptr) {
=======
  auto reservation_it = reserved_descriptor_tracks_.find(counter_track_uuid);
  if (reservation_it == reserved_descriptor_tracks_.end()) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    PERFETTO_DLOG("Unknown counter track with uuid %" PRIu64,
                  counter_track_uuid);
    return std::nullopt;
  }

<<<<<<< HEAD
  DescriptorTrackReservation& reservation = *reservation_ptr;
=======
  DescriptorTrackReservation& reservation = reservation_it->second;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  if (!reservation.is_counter) {
    PERFETTO_DLOG("Track with uuid %" PRIu64 " is not a counter track",
                  counter_track_uuid);
    return std::nullopt;
  }
<<<<<<< HEAD
  if (!reservation.counter_details) {
    PERFETTO_FATAL("Counter tracks require `counter_details`.");
  }
  DescriptorTrackReservation::CounterDetails& c_details =
      *reservation.counter_details;

  if (c_details.unit_multiplier > 0)
    value *= static_cast<double>(c_details.unit_multiplier);

  if (c_details.is_incremental) {
    if (c_details.packet_sequence_id != packet_sequence_id) {
=======

  if (reservation.unit_multiplier > 0)
    value *= static_cast<double>(reservation.unit_multiplier);

  if (reservation.is_incremental) {
    if (reservation.packet_sequence_id != packet_sequence_id) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      PERFETTO_DLOG(
          "Incremental counter track with uuid %" PRIu64
          " was updated from the wrong packet sequence (expected: %" PRIu32
          " got:%" PRIu32 ")",
<<<<<<< HEAD
          counter_track_uuid, c_details.packet_sequence_id, packet_sequence_id);
      return std::nullopt;
    }

    c_details.latest_value += value;
    value = c_details.latest_value;
  }
=======
          counter_track_uuid, reservation.packet_sequence_id,
          packet_sequence_id);
      return std::nullopt;
    }

    reservation.latest_value += value;
    value = reservation.latest_value;
  }

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  return value;
}

void TrackEventTracker::OnIncrementalStateCleared(uint32_t packet_sequence_id) {
  // TODO(eseckler): Improve on the runtime complexity of this. At O(hundreds)
  // of packet sequences, incremental state clearing at O(trace second), and
  // total number of tracks in O(thousands), a linear scan through all tracks
  // here might not be fast enough.
<<<<<<< HEAD
  for (auto it = reserved_descriptor_tracks_.GetIterator(); it; ++it) {
    DescriptorTrackReservation& reservation = it.value();
    // Only consider incremental counter tracks for current sequence.
    if (!reservation.is_counter || !reservation.counter_details ||
        !reservation.counter_details->is_incremental ||
        reservation.counter_details->packet_sequence_id != packet_sequence_id) {
      continue;
    }
    // Reset their value to 0, see CounterDescriptor's |is_incremental|.
    reservation.counter_details->latest_value = 0;
=======
  for (auto& entry : reserved_descriptor_tracks_) {
    DescriptorTrackReservation& reservation = entry.second;
    // Only consider incremental counter tracks for current sequence.
    if (!reservation.is_counter || !reservation.is_incremental ||
        reservation.packet_sequence_id != packet_sequence_id) {
      continue;
    }
    // Reset their value to 0, see CounterDescriptor's |is_incremental|.
    reservation.latest_value = 0;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }
}

void TrackEventTracker::OnFirstPacketOnSequence(uint32_t packet_sequence_id) {
  sequences_with_first_packet_.insert(packet_sequence_id);
}

<<<<<<< HEAD
void TrackEventTracker::AddTrackArgs(
    uint64_t uuid,
    std::optional<uint32_t> packet_sequence_id,
    const DescriptorTrackReservation& reservation,
    bool is_root_in_scope,
    ArgsTracker::BoundInserter& args) {
  args.AddArg(source_key_, Variadic::String(descriptor_source_))
      .AddArg(source_id_key_, Variadic::Integer(static_cast<int64_t>(uuid)))
      .AddArg(is_root_in_scope_key_, Variadic::Boolean(is_root_in_scope));
  if (reservation.counter_details) {
    if (!reservation.counter_details->category.is_null()) {
      args.AddArg(category_key_,
                  Variadic::String(reservation.counter_details->category));
    }
    if (!reservation.counter_details->builtin_type_str.is_null()) {
      args.AddArg(
          builtin_counter_type_key_,
          Variadic::String(reservation.counter_details->builtin_type_str));
    }
  }
  if (packet_sequence_id &&
      sequences_with_first_packet_.find(*packet_sequence_id) !=
          sequences_with_first_packet_.end()) {
    args.AddArg(has_first_packet_on_sequence_key_id_, Variadic::Boolean(true));
  }

  switch (reservation.ordering) {
    case DescriptorTrackReservation::ChildTracksOrdering::kLexicographic:
      args.AddArg(child_ordering_key_, Variadic::String(lexicographic_id_));
      break;
    case DescriptorTrackReservation::ChildTracksOrdering::kChronological:
      args.AddArg(child_ordering_key_, Variadic::String(chronological_id_));
      break;
    case DescriptorTrackReservation::ChildTracksOrdering::kExplicit:
      args.AddArg(child_ordering_key_, Variadic::String(explicit_id_));
      break;
    case DescriptorTrackReservation::ChildTracksOrdering::kUnknown:
      break;
  }

  if (reservation.sibling_order_rank) {
    args.AddArg(sibling_order_rank_key_,
                Variadic::Integer(*reservation.sibling_order_rank));
  }
}

bool TrackEventTracker::IsTrackHierarchyValid(uint64_t uuid) {
  // Do a basic tree walking algorithm to find if there are duplicate ids or
  // the path to the root is longer than kMaxAncestors.
  static constexpr size_t kMaxAncestors = 100;
  std::array<uint64_t, kMaxAncestors> seen;
  uint64_t current_uuid = uuid;
  for (uint32_t i = 0; i < kMaxAncestors; ++i) {
    if (current_uuid == 0) {
      return true;
    }
    for (uint32_t j = 0; j < i; ++j) {
      if (current_uuid == seen[j]) {
        PERFETTO_ELOG("Loop detected in hierarchy for track %" PRIu64, uuid);
        return false;
      }
    }
    auto* reservation_ptr = reserved_descriptor_tracks_.Find(current_uuid);
    if (!reservation_ptr) {
      PERFETTO_ELOG("Missing uuid in hierarchy for track %" PRIu64, uuid);
      return false;
    }
    seen[i] = current_uuid;
    current_uuid = reservation_ptr->parent_uuid;
  }
  PERFETTO_ELOG("Too many ancestors in hierarchy for track %" PRIu64, uuid);
  return false;
}

TrackEventTracker::ResolvedDescriptorTrack
TrackEventTracker::ResolvedDescriptorTrack::Process(TrackId track_id,
                                                    UniquePid upid,
                                                    bool is_counter,
                                                    bool is_root) {
  ResolvedDescriptorTrack track;
  track.track_id_ = track_id;
  track.scope_ = Scope::kProcess;
  track.is_counter_ = is_counter;
  track.upid_ = upid;
  track.is_root_ = is_root;
=======
TrackEventTracker::ResolvedDescriptorTrack
TrackEventTracker::ResolvedDescriptorTrack::Process(UniquePid upid,
                                                    bool is_counter,
                                                    bool is_root) {
  ResolvedDescriptorTrack track;
  track.scope_ = Scope::kProcess;
  track.is_counter_ = is_counter;
  track.is_root_in_scope_ = is_root;
  track.upid_ = upid;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  return track;
}

TrackEventTracker::ResolvedDescriptorTrack
<<<<<<< HEAD
TrackEventTracker::ResolvedDescriptorTrack::Thread(TrackId track_id,
                                                   UniqueTid utid,
                                                   bool is_counter,
                                                   bool is_root) {
  ResolvedDescriptorTrack track;
  track.track_id_ = track_id;
  track.scope_ = Scope::kThread;
  track.is_counter_ = is_counter;
  track.utid_ = utid;
  track.is_root_ = is_root;
=======
TrackEventTracker::ResolvedDescriptorTrack::Thread(UniqueTid utid,
                                                   bool is_counter,
                                                   bool is_root,
                                                   bool use_separate_track) {
  ResolvedDescriptorTrack track;
  track.scope_ = Scope::kThread;
  track.is_counter_ = is_counter;
  track.is_root_in_scope_ = is_root;
  track.utid_ = utid;
  track.use_separate_track_ = use_separate_track;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  return track;
}

TrackEventTracker::ResolvedDescriptorTrack
<<<<<<< HEAD
TrackEventTracker::ResolvedDescriptorTrack::Global(TrackId track_id,
                                                   bool is_counter) {
  ResolvedDescriptorTrack track;
  track.track_id_ = track_id;
  track.scope_ = Scope::kGlobal;
  track.is_counter_ = is_counter;
  track.is_root_ = false;
  return track;
}

}  // namespace perfetto::trace_processor
=======
TrackEventTracker::ResolvedDescriptorTrack::Global(bool is_counter,
                                                   bool is_root) {
  ResolvedDescriptorTrack track;
  track.scope_ = Scope::kGlobal;
  track.is_counter_ = is_counter;
  track.is_root_in_scope_ = is_root;
  return track;
}

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
