/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/chrome_system_probes_parser.h"

<<<<<<< HEAD
#include <cstdint>

=======
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "perfetto/protozero/proto_decoder.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
<<<<<<< HEAD
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/common/tracks_common.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto::trace_processor {
=======
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/ps/process_stats.pbzero.h"
#include "protos/perfetto/trace/ps/process_tree.pbzero.h"
#include "protos/perfetto/trace/sys_stats/sys_stats.pbzero.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

ChromeSystemProbesParser::ChromeSystemProbesParser(
    TraceProcessorContext* context)
    : context_(context),
      is_peak_rss_resettable_id_(
<<<<<<< HEAD
          context->storage->InternString("is_peak_rss_resettable")) {}

void ChromeSystemProbesParser::ParseProcessStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::ProcessStats::Decoder stats(blob);
  for (auto it = stats.processes(); it; ++it) {
    protozero::ProtoDecoder proc(*it);
    auto pid_field =
        proc.FindField(protos::pbzero::ProcessStats::Process::kPidFieldNumber);
    uint32_t pid = pid_field.as_uint32();

    for (auto fld = proc.ReadField(); fld.valid(); fld = proc.ReadField()) {
      using ProcessStats = protos::pbzero::ProcessStats;
      if (fld.id() == ProcessStats::Process::kIsPeakRssResettableFieldNumber) {
=======
          context->storage->InternString("is_peak_rss_resettable")) {
  using ProcessStats = protos::pbzero::ProcessStats;
  proc_stats_process_names_
      [ProcessStats::Process::kChromePrivateFootprintKbFieldNumber] =
          context->storage->InternString("chrome.private_footprint_kb");
  proc_stats_process_names_
      [ProcessStats::Process::kChromePeakResidentSetKbFieldNumber] =
          context->storage->InternString("chrome.peak_resident_set_kb");
}

void ChromeSystemProbesParser::ParseProcessStats(int64_t ts, ConstBytes blob) {
  protos::pbzero::ProcessStats::Decoder stats(blob.data, blob.size);
  for (auto it = stats.processes(); it; ++it) {
    protozero::ProtoDecoder proc(*it);
    uint32_t pid = 0;
    for (auto fld = proc.ReadField(); fld.valid(); fld = proc.ReadField()) {
      if (fld.id() == protos::pbzero::ProcessStats::Process::kPidFieldNumber) {
        pid = fld.as_uint32();
        break;
      }
    }

    for (auto fld = proc.ReadField(); fld.valid(); fld = proc.ReadField()) {
      if (fld.id() == protos::pbzero::ProcessStats::Process::
                          kIsPeakRssResettableFieldNumber) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        UniquePid upid = context_->process_tracker->GetOrCreateProcess(pid);
        context_->process_tracker->AddArgsTo(upid).AddArg(
            is_peak_rss_resettable_id_, Variadic::Boolean(fld.as_bool()));
        continue;
      }
<<<<<<< HEAD
      if (fld.id() ==
          ProcessStats::Process::kChromePrivateFootprintKbFieldNumber) {
        UniquePid upid = context_->process_tracker->GetOrCreateProcess(pid);
        TrackId track = context_->track_tracker->InternTrack(
            tracks::kChromeProcessStatsBlueprint,
            tracks::Dimensions(upid, "private_footprint_kb"));
        int64_t value = fld.as_int64() * 1024;
        context_->event_tracker->PushCounter(ts, static_cast<double>(value),
                                             track);
        continue;
      }
      if (fld.id() ==
          ProcessStats::Process::kChromePeakResidentSetKbFieldNumber) {
        UniquePid upid = context_->process_tracker->GetOrCreateProcess(pid);
        TrackId track = context_->track_tracker->InternTrack(
            tracks::kChromeProcessStatsBlueprint,
            tracks::Dimensions(upid, "peak_resident_set_kb"));
        int64_t value = fld.as_int64() * 1024;
        context_->event_tracker->PushCounter(ts, static_cast<double>(value),
                                             track);
        continue;
      }
=======

      if (fld.id() >= proc_stats_process_names_.size())
        continue;
      const StringId& name = proc_stats_process_names_[fld.id()];
      if (name == StringId::Null())
        continue;
      UniquePid upid = context_->process_tracker->GetOrCreateProcess(pid);
      TrackId track =
          context_->track_tracker->InternProcessCounterTrack(name, upid);
      int64_t value = fld.as_int64() * 1024;
      context_->event_tracker->PushCounter(ts, static_cast<double>(value),
                                           track);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    }
  }
}

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
