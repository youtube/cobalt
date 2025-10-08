/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/importers/common/track_tracker.h"

<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>

#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/cpu_tracker.h"
#include "src/trace_processor/importers/common/process_track_translation_table.h"
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/common/tracks_common.h"
#include "src/trace_processor/importers/common/tracks_internal.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/track_tables_py.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto::trace_processor {

TrackTracker::TrackTracker(TraceProcessorContext* context)
    : source_key_(context->storage->InternString("source")),
      trace_id_key_(context->storage->InternString("trace_id")),
      trace_id_is_process_scoped_key_(
          context->storage->InternString("trace_id_is_process_scoped")),
      upid_(context->storage->InternString("upid")),
      source_scope_key_(context->storage->InternString("source_scope")),
      chrome_source_(context->storage->InternString("chrome")),
      context_(context),
      args_tracker_(context) {}

TrackId TrackTracker::InternLegacyAsyncTrack(StringId raw_name,
                                             uint32_t upid,
                                             int64_t trace_id,
                                             bool trace_id_is_process_scoped,
                                             StringId source_scope) {
  const StringId name =
      context_->process_track_translation_table->TranslateName(raw_name);

  auto args_fn = [&](ArgsTracker::BoundInserter& inserter) {
    inserter.AddArg(source_key_, Variadic::String(chrome_source_))
        .AddArg(trace_id_key_, Variadic::Integer(trace_id))
        .AddArg(trace_id_is_process_scoped_key_,
                Variadic::Boolean(trace_id_is_process_scoped))
        .AddArg(upid_, Variadic::UnsignedInteger(upid))
        .AddArg(source_scope_key_, Variadic::String(source_scope));
  };
  TrackId track_id;
  bool inserted;
  if (trace_id_is_process_scoped) {
    static constexpr auto kBlueprint = tracks::SliceBlueprint(
        "legacy_async_process_slice",
        tracks::DimensionBlueprints(tracks::kProcessDimensionBlueprint,
                                    tracks::StringDimensionBlueprint("scope"),
                                    tracks::LongDimensionBlueprint("cookie")),
        tracks::DynamicNameBlueprint());
    std::tie(track_id, inserted) = InternTrackInner(
        kBlueprint,
        tracks::Dimensions(upid, context_->storage->GetString(source_scope),
                           trace_id),
        tracks::DynamicName(name), args_fn);
  } else {
    static constexpr auto kBlueprint = tracks::SliceBlueprint(
        "legacy_async_global_slice",
        tracks::DimensionBlueprints(tracks::StringDimensionBlueprint("scope"),
                                    tracks::LongDimensionBlueprint("cookie")),
        tracks::DynamicNameBlueprint());
    std::tie(track_id, inserted) = InternTrackInner(
        kBlueprint,
        tracks::Dimensions(context_->storage->GetString(source_scope),
                           trace_id),
        tracks::DynamicName(name), args_fn);
  }
  // The track may have been created for an end event without name. In
  // that case, update it with this event's name.
  if (inserted && name != kNullStringId) {
    auto& tracks = *context_->storage->mutable_track_table();
    auto rr = *tracks.FindById(track_id);
    if (rr.name() == kNullStringId) {
      rr.set_name(name);
    }
  }
  return track_id;
}

TrackId TrackTracker::AddTrack(const tracks::BlueprintBase& blueprint,
                               StringId name,
                               StringId counter_unit,
                               GlobalArgsTracker::CompactArg* d_args,
                               uint32_t d_size,
                               const SetArgsCallback& args) {
  tables::TrackTable::Row row(name);
  const auto* dims = blueprint.dimension_blueprints.data();
  for (uint32_t i = 0; i < d_size; ++i) {
    base::StringView str(dims[i].name.data(), dims[i].name.size());
    if (str == "cpu" && d_args[i].value.type == Variadic::kInt) {
      context_->cpu_tracker->MarkCpuValid(
          static_cast<uint32_t>(d_args[i].value.int_value));
    } else if (str == "utid" && d_args[i].value.type == Variadic::kInt) {
      row.utid = static_cast<uint32_t>(d_args[i].value.int_value);
    } else if (str == "upid" && d_args[i].value.type == Variadic::kInt) {
      row.upid = static_cast<uint32_t>(d_args[i].value.int_value);
    }
    StringId key = context_->storage->InternString(str);
    d_args[i].key = key;
    d_args[i].flat_key = key;
  }

  row.machine_id = context_->machine_id();
  row.type = context_->storage->InternString(
      base::StringView(blueprint.type.data(), blueprint.type.size()));
  if (d_size > 0) {
    row.dimension_arg_set_id =
        context_->global_args_tracker->AddArgSet(d_args, 0, d_size);
  }
  row.event_type = context_->storage->InternString(blueprint.event_type);
  row.counter_unit = counter_unit;
  TrackId id = context_->storage->mutable_track_table()->Insert(row).id;
  if (args) {
    auto inserter = args_tracker_.AddArgsTo(id);
    args(inserter);
    args_tracker_.Flush();
=======
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"

namespace perfetto {
namespace trace_processor {

TrackTracker::TrackTracker(TraceProcessorContext* context)
    : source_key_(context->storage->InternString("source")),
      source_id_key_(context->storage->InternString("source_id")),
      source_id_is_process_scoped_key_(
          context->storage->InternString("source_id_is_process_scoped")),
      source_scope_key_(context->storage->InternString("source_scope")),
      category_key_(context->storage->InternString("category")),
      fuchsia_source_(context->storage->InternString("fuchsia")),
      chrome_source_(context->storage->InternString("chrome")),
      context_(context) {}

TrackId TrackTracker::InternThreadTrack(UniqueTid utid) {
  auto it = thread_tracks_.find(utid);
  if (it != thread_tracks_.end())
    return it->second;

  tables::ThreadTrackTable::Row row;
  row.utid = utid;
  auto id = context_->storage->mutable_thread_track_table()->Insert(row).id;
  thread_tracks_[utid] = id;
  return id;
}

TrackId TrackTracker::InternProcessTrack(UniquePid upid) {
  auto it = process_tracks_.find(upid);
  if (it != process_tracks_.end())
    return it->second;

  tables::ProcessTrackTable::Row row;
  row.upid = upid;
  auto id = context_->storage->mutable_process_track_table()->Insert(row).id;
  process_tracks_[upid] = id;
  return id;
}

TrackId TrackTracker::InternFuchsiaAsyncTrack(StringId name,
                                              uint32_t upid,
                                              int64_t correlation_id) {
  return InternLegacyChromeAsyncTrack(name, upid, correlation_id, false,
                                      StringId());
}

TrackId TrackTracker::InternCpuTrack(StringId name, uint32_t cpu) {
  auto it = cpu_tracks_.find(std::make_pair(name, cpu));
  if (it != cpu_tracks_.end()) {
    return it->second;
  }

  tables::CpuTrackTable::Row row(name);
  row.cpu = cpu;
  auto id = context_->storage->mutable_cpu_track_table()->Insert(row).id;
  cpu_tracks_[std::make_pair(name, cpu)] = id;

  return id;
}

TrackId TrackTracker::InternGpuTrack(const tables::GpuTrackTable::Row& row) {
  GpuTrackTuple tuple{row.name, row.scope, row.context_id.value_or(0)};

  auto it = gpu_tracks_.find(tuple);
  if (it != gpu_tracks_.end())
    return it->second;

  auto id = context_->storage->mutable_gpu_track_table()->Insert(row).id;
  gpu_tracks_[tuple] = id;
  return id;
}

TrackId TrackTracker::InternLegacyChromeAsyncTrack(
    StringId name,
    uint32_t upid,
    int64_t source_id,
    bool source_id_is_process_scoped,
    StringId source_scope) {
  ChromeTrackTuple tuple;
  if (source_id_is_process_scoped)
    tuple.upid = upid;
  tuple.source_id = source_id;
  tuple.source_scope = source_scope;

  auto it = chrome_tracks_.find(tuple);
  if (it != chrome_tracks_.end()) {
    if (name != kNullStringId) {
      // The track may have been created for an end event without name. In that
      // case, update it with this event's name.
      auto* tracks = context_->storage->mutable_track_table();
      uint32_t track_row = *tracks->id().IndexOf(it->second);
      if (tracks->name()[track_row] == kNullStringId)
        tracks->mutable_name()->Set(track_row, name);
    }
    return it->second;
  }

  // Legacy async tracks are always drawn in the context of a process, even if
  // the ID's scope is global.
  tables::ProcessTrackTable::Row track(name);
  track.upid = upid;
  TrackId id =
      context_->storage->mutable_process_track_table()->Insert(track).id;
  chrome_tracks_[tuple] = id;

  context_->args_tracker->AddArgsTo(id)
      .AddArg(source_key_, Variadic::String(chrome_source_))
      .AddArg(source_id_key_, Variadic::Integer(source_id))
      .AddArg(source_id_is_process_scoped_key_,
              Variadic::Boolean(source_id_is_process_scoped))
      .AddArg(source_scope_key_, Variadic::String(source_scope));

  return id;
}

TrackId TrackTracker::CreateGlobalAsyncTrack(StringId name, StringId source) {
  tables::TrackTable::Row row(name);
  auto id = context_->storage->mutable_track_table()->Insert(row).id;
  if (!source.is_null()) {
    context_->args_tracker->AddArgsTo(id).AddArg(source_key_,
                                                 Variadic::String(source));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }
  return id;
}

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
TrackId TrackTracker::CreateProcessAsyncTrack(StringId name,
                                              UniquePid upid,
                                              StringId source) {
  tables::ProcessTrackTable::Row row(name);
  row.upid = upid;
  auto id = context_->storage->mutable_process_track_table()->Insert(row).id;
  if (!source.is_null()) {
    context_->args_tracker->AddArgsTo(id).AddArg(source_key_,
                                                 Variadic::String(source));
  }
  return id;
}

TrackId TrackTracker::InternLegacyChromeProcessInstantTrack(UniquePid upid) {
  auto it = chrome_process_instant_tracks_.find(upid);
  if (it != chrome_process_instant_tracks_.end())
    return it->second;

  tables::ProcessTrackTable::Row row;
  row.upid = upid;
  auto id = context_->storage->mutable_process_track_table()->Insert(row).id;
  chrome_process_instant_tracks_[upid] = id;

  context_->args_tracker->AddArgsTo(id).AddArg(
      source_key_, Variadic::String(chrome_source_));

  return id;
}

TrackId TrackTracker::GetOrCreateLegacyChromeGlobalInstantTrack() {
  if (!chrome_global_instant_track_id_) {
    chrome_global_instant_track_id_ =
        context_->storage->mutable_track_table()->Insert({}).id;

    context_->args_tracker->AddArgsTo(*chrome_global_instant_track_id_)
        .AddArg(source_key_, Variadic::String(chrome_source_));
  }
  return *chrome_global_instant_track_id_;
}

TrackId TrackTracker::GetOrCreateTriggerTrack() {
  if (trigger_track_id_) {
    return *trigger_track_id_;
  }
  tables::TrackTable::Row row;
  row.name = context_->storage->InternString("Trace Triggers");
  trigger_track_id_ = context_->storage->mutable_track_table()->Insert(row).id;
  return *trigger_track_id_;
}

TrackId TrackTracker::InternGlobalCounterTrack(StringId name,
                                               SetArgsCallback callback,
                                               StringId unit,
                                               StringId description) {
  auto it = global_counter_tracks_by_name_.find(name);
  if (it != global_counter_tracks_by_name_.end()) {
    return it->second;
  }

  tables::CounterTrackTable::Row row(name);
  row.unit = unit;
  row.description = description;
  TrackId track =
      context_->storage->mutable_counter_track_table()->Insert(row).id;
  global_counter_tracks_by_name_[name] = track;
  if (callback) {
    auto inserter = context_->args_tracker->AddArgsTo(track);
    callback(inserter);
  }
  return track;
}

TrackId TrackTracker::InternCpuCounterTrack(StringId name, uint32_t cpu) {
  auto it = cpu_counter_tracks_.find(std::make_pair(name, cpu));
  if (it != cpu_counter_tracks_.end()) {
    return it->second;
  }

  tables::CpuCounterTrackTable::Row row(name);
  row.cpu = cpu;

  TrackId track =
      context_->storage->mutable_cpu_counter_track_table()->Insert(row).id;
  cpu_counter_tracks_[std::make_pair(name, cpu)] = track;
  return track;
}

TrackId TrackTracker::InternThreadCounterTrack(StringId name, UniqueTid utid) {
  auto it = utid_counter_tracks_.find(std::make_pair(name, utid));
  if (it != utid_counter_tracks_.end()) {
    return it->second;
  }

  tables::ThreadCounterTrackTable::Row row(name);
  row.utid = utid;

  TrackId track =
      context_->storage->mutable_thread_counter_track_table()->Insert(row).id;
  utid_counter_tracks_[std::make_pair(name, utid)] = track;
  return track;
}

TrackId TrackTracker::InternProcessCounterTrack(StringId name,
                                                UniquePid upid,
                                                StringId unit,
                                                StringId description) {
  auto it = upid_counter_tracks_.find(std::make_pair(name, upid));
  if (it != upid_counter_tracks_.end()) {
    return it->second;
  }

  tables::ProcessCounterTrackTable::Row row(name);
  row.upid = upid;
  row.unit = unit;
  row.description = description;

  TrackId track =
      context_->storage->mutable_process_counter_track_table()->Insert(row).id;
  upid_counter_tracks_[std::make_pair(name, upid)] = track;
  return track;
}

TrackId TrackTracker::InternIrqCounterTrack(StringId name, int32_t irq) {
  auto it = irq_counter_tracks_.find(std::make_pair(name, irq));
  if (it != irq_counter_tracks_.end()) {
    return it->second;
  }

  tables::IrqCounterTrackTable::Row row(name);
  row.irq = irq;

  TrackId track =
      context_->storage->mutable_irq_counter_track_table()->Insert(row).id;
  irq_counter_tracks_[std::make_pair(name, irq)] = track;
  return track;
}

TrackId TrackTracker::InternSoftirqCounterTrack(StringId name,
                                                int32_t softirq) {
  auto it = softirq_counter_tracks_.find(std::make_pair(name, softirq));
  if (it != softirq_counter_tracks_.end()) {
    return it->second;
  }

  tables::SoftirqCounterTrackTable::Row row(name);
  row.softirq = softirq;

  TrackId track =
      context_->storage->mutable_softirq_counter_track_table()->Insert(row).id;
  softirq_counter_tracks_[std::make_pair(name, softirq)] = track;
  return track;
}

TrackId TrackTracker::InternGpuCounterTrack(StringId name, uint32_t gpu_id) {
  auto it = gpu_counter_tracks_.find(std::make_pair(name, gpu_id));
  if (it != gpu_counter_tracks_.end()) {
    return it->second;
  }
  TrackId track = CreateGpuCounterTrack(name, gpu_id);
  gpu_counter_tracks_[std::make_pair(name, gpu_id)] = track;
  return track;
}

TrackId TrackTracker::InternEnergyCounterTrack(StringId name,
                                               int32_t consumer_id,
                                               StringId consumer_type,
                                               int32_t ordinal) {
  auto it = energy_counter_tracks_.find(std::make_pair(name, consumer_id));
  if (it != energy_counter_tracks_.end()) {
    return it->second;
  }
  tables::EnergyCounterTrackTable::Row row(name);
  row.consumer_id = consumer_id;
  row.consumer_type = consumer_type;
  row.ordinal = ordinal;
  TrackId track =
      context_->storage->mutable_energy_counter_track_table()->Insert(row).id;
  energy_counter_tracks_[std::make_pair(name, consumer_id)] = track;
  return track;
}

TrackId TrackTracker::InternUidCounterTrack(StringId name, int32_t uid) {
  auto it = uid_counter_tracks_.find(std::make_pair(name, uid));
  if (it != uid_counter_tracks_.end()) {
    return it->second;
  }

  tables::UidCounterTrackTable::Row row(name);
  row.uid = uid;
  TrackId track =
      context_->storage->mutable_uid_counter_track_table()->Insert(row).id;
  uid_counter_tracks_[std::make_pair(name, uid)] = track;
  return track;
}

TrackId TrackTracker::InternEnergyPerUidCounterTrack(StringId name,
                                                     int32_t consumer_id,
                                                     int32_t uid) {
  auto it = energy_per_uid_counter_tracks_.find(std::make_pair(name, uid));
  if (it != energy_per_uid_counter_tracks_.end()) {
    return it->second;
  }

  tables::EnergyPerUidCounterTrackTable::Row row(name);
  row.consumer_id = consumer_id;
  row.uid = uid;
  TrackId track =
      context_->storage->mutable_energy_per_uid_counter_track_table()
          ->Insert(row)
          .id;
  energy_per_uid_counter_tracks_[std::make_pair(name, uid)] = track;
  return track;
}

TrackId TrackTracker::CreateGpuCounterTrack(StringId name,
                                            uint32_t gpu_id,
                                            StringId description,
                                            StringId unit) {
  tables::GpuCounterTrackTable::Row row(name);
  row.gpu_id = gpu_id;
  row.description = description;
  row.unit = unit;

  return context_->storage->mutable_gpu_counter_track_table()->Insert(row).id;
}

TrackId TrackTracker::CreatePerfCounterTrack(StringId name,
                                             uint32_t perf_session_id,
                                             uint32_t cpu,
                                             bool is_timebase) {
  tables::PerfCounterTrackTable::Row row(name);
  row.perf_session_id = perf_session_id;
  row.cpu = cpu;
  row.is_timebase = is_timebase;
  return context_->storage->mutable_perf_counter_track_table()->Insert(row).id;
}

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
