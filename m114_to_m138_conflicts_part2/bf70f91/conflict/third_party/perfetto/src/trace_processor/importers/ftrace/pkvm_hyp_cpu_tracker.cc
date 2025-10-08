/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "src/trace_processor/importers/ftrace/pkvm_hyp_cpu_tracker.h"

<<<<<<< HEAD
#include <cstdint>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/common/tracks_common.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/hyp.pbzero.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto::trace_processor {
namespace {

constexpr auto kPkvmBlueprint = tracks::SliceBlueprint(
    "pkvm_hypervisor",
    tracks::DimensionBlueprints(tracks::kCpuDimensionBlueprint),
    tracks::FnNameBlueprint([](uint32_t cpu) {
      return base::StackString<255>("pkVM Hypervisor CPU %u", cpu);
    }));

}  // namespace
=======
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/hyp.pbzero.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

PkvmHypervisorCpuTracker::PkvmHypervisorCpuTracker(
    TraceProcessorContext* context)
    : context_(context),
      category_(context->storage->InternString("pkvm_hyp")),
      slice_name_(context->storage->InternString("hyp")),
      hyp_enter_reason_(context->storage->InternString("hyp_enter_reason")) {}

// static
<<<<<<< HEAD
bool PkvmHypervisorCpuTracker::IsPkvmHypervisorEvent(uint32_t event_id) {
=======
bool PkvmHypervisorCpuTracker::IsPkvmHypervisorEvent(uint16_t event_id) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  using protos::pbzero::FtraceEvent;
  switch (event_id) {
    case FtraceEvent::kHypEnterFieldNumber:
    case FtraceEvent::kHypExitFieldNumber:
    case FtraceEvent::kHostHcallFieldNumber:
    case FtraceEvent::kHostMemAbortFieldNumber:
    case FtraceEvent::kHostSmcFieldNumber:
      return true;
    default:
      return false;
  }
}

void PkvmHypervisorCpuTracker::ParseHypEvent(uint32_t cpu,
                                             int64_t timestamp,
<<<<<<< HEAD
                                             uint32_t event_id,
=======
                                             uint16_t event_id,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                                             protozero::ConstBytes blob) {
  using protos::pbzero::FtraceEvent;
  switch (event_id) {
    case FtraceEvent::kHypEnterFieldNumber:
      ParseHypEnter(cpu, timestamp);
      break;
    case FtraceEvent::kHypExitFieldNumber:
      ParseHypExit(cpu, timestamp);
      break;
    case FtraceEvent::kHostHcallFieldNumber:
      ParseHostHcall(cpu, blob);
      break;
    case FtraceEvent::kHostMemAbortFieldNumber:
      ParseHostMemAbort(cpu, blob);
      break;
    case FtraceEvent::kHostSmcFieldNumber:
      ParseHostSmc(cpu, blob);
      break;
    // TODO(b/249050813): add remaining hypervisor events
    default:
<<<<<<< HEAD
      PERFETTO_FATAL("Not a hypervisor event %u", event_id);
=======
      PERFETTO_FATAL("Not a hypervisor event %d", event_id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }
}

void PkvmHypervisorCpuTracker::ParseHypEnter(uint32_t cpu, int64_t timestamp) {
  // TODO(b/249050813): handle bad events (e.g. 2 hyp_enter in a row)
<<<<<<< HEAD
  TrackId track_id = context_->track_tracker->InternTrack(
      kPkvmBlueprint, tracks::Dimensions(cpu));
=======

  TrackId track_id = GetHypCpuTrackId(cpu);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  context_->slice_tracker->Begin(timestamp, track_id, category_, slice_name_);
}

void PkvmHypervisorCpuTracker::ParseHypExit(uint32_t cpu, int64_t timestamp) {
  // TODO(b/249050813): handle bad events (e.g. 2 hyp_exit in a row)
<<<<<<< HEAD
  TrackId track_id = context_->track_tracker->InternTrack(
      kPkvmBlueprint, tracks::Dimensions(cpu));
=======
  TrackId track_id = GetHypCpuTrackId(cpu);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  context_->slice_tracker->End(timestamp, track_id);
}

void PkvmHypervisorCpuTracker::ParseHostHcall(uint32_t cpu,
                                              protozero::ConstBytes blob) {
<<<<<<< HEAD
  protos::pbzero::HostHcallFtraceEvent::Decoder evt(blob);
  TrackId track_id = context_->track_tracker->InternTrack(
      kPkvmBlueprint, tracks::Dimensions(cpu));
  context_->slice_tracker->AddArgs(
      track_id, category_, slice_name_,
      [&, this](ArgsTracker::BoundInserter* inserter) {
        StringId host_hcall = context_->storage->InternString("host_hcall");
        StringId id = context_->storage->InternString("id");
        StringId invalid = context_->storage->InternString("invalid");
        inserter->AddArg(hyp_enter_reason_, Variadic::String(host_hcall));
        inserter->AddArg(id, Variadic::UnsignedInteger(evt.id()));
        inserter->AddArg(invalid, Variadic::UnsignedInteger(evt.invalid()));
      });
=======
  protos::pbzero::HostHcallFtraceEvent::Decoder evt(blob.data, blob.size);
  TrackId track_id = GetHypCpuTrackId(cpu);

  auto args_inserter = [this, &evt](ArgsTracker::BoundInserter* inserter) {
    StringId host_hcall = context_->storage->InternString("host_hcall");
    StringId id = context_->storage->InternString("id");
    StringId invalid = context_->storage->InternString("invalid");
    inserter->AddArg(hyp_enter_reason_, Variadic::String(host_hcall));
    inserter->AddArg(id, Variadic::UnsignedInteger(evt.id()));
    inserter->AddArg(invalid, Variadic::UnsignedInteger(evt.invalid()));
  };
  context_->slice_tracker->AddArgs(track_id, category_, slice_name_,
                                   args_inserter);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

void PkvmHypervisorCpuTracker::ParseHostSmc(uint32_t cpu,
                                            protozero::ConstBytes blob) {
<<<<<<< HEAD
  protos::pbzero::HostSmcFtraceEvent::Decoder evt(blob);
  TrackId track_id = context_->track_tracker->InternTrack(
      kPkvmBlueprint, tracks::Dimensions(cpu));
  context_->slice_tracker->AddArgs(
      track_id, category_, slice_name_,
      [&, this](ArgsTracker::BoundInserter* inserter) {
        StringId host_smc = context_->storage->InternString("host_smc");
        StringId id = context_->storage->InternString("id");
        StringId forwarded = context_->storage->InternString("forwarded");
        inserter->AddArg(hyp_enter_reason_, Variadic::String(host_smc));
        inserter->AddArg(id, Variadic::UnsignedInteger(evt.id()));
        inserter->AddArg(forwarded, Variadic::UnsignedInteger(evt.forwarded()));
      });
=======
  protos::pbzero::HostSmcFtraceEvent::Decoder evt(blob.data, blob.size);
  TrackId track_id = GetHypCpuTrackId(cpu);

  auto args_inserter = [this, &evt](ArgsTracker::BoundInserter* inserter) {
    StringId host_smc = context_->storage->InternString("host_smc");
    StringId id = context_->storage->InternString("id");
    StringId forwarded = context_->storage->InternString("forwarded");
    inserter->AddArg(hyp_enter_reason_, Variadic::String(host_smc));
    inserter->AddArg(id, Variadic::UnsignedInteger(evt.id()));
    inserter->AddArg(forwarded, Variadic::UnsignedInteger(evt.forwarded()));
  };
  context_->slice_tracker->AddArgs(track_id, category_, slice_name_,
                                   args_inserter);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

void PkvmHypervisorCpuTracker::ParseHostMemAbort(uint32_t cpu,
                                                 protozero::ConstBytes blob) {
<<<<<<< HEAD
  protos::pbzero::HostMemAbortFtraceEvent::Decoder evt(blob);
  TrackId track_id = context_->track_tracker->InternTrack(
      kPkvmBlueprint, tracks::Dimensions(cpu));
  context_->slice_tracker->AddArgs(
      track_id, category_, slice_name_,
      [&, this](ArgsTracker::BoundInserter* inserter) {
        StringId host_mem_abort =
            context_->storage->InternString("host_mem_abort");
        StringId esr = context_->storage->InternString("esr");
        StringId addr = context_->storage->InternString("addr");
        inserter->AddArg(hyp_enter_reason_, Variadic::String(host_mem_abort));
        inserter->AddArg(esr, Variadic::UnsignedInteger(evt.esr()));
        inserter->AddArg(addr, Variadic::UnsignedInteger(evt.addr()));
      });
}

}  // namespace perfetto::trace_processor
=======
  protos::pbzero::HostMemAbortFtraceEvent::Decoder evt(blob.data, blob.size);
  TrackId track_id = GetHypCpuTrackId(cpu);

  auto args_inserter = [this, &evt](ArgsTracker::BoundInserter* inserter) {
    StringId host_mem_abort = context_->storage->InternString("host_mem_abort");
    StringId esr = context_->storage->InternString("esr");
    StringId addr = context_->storage->InternString("addr");
    inserter->AddArg(hyp_enter_reason_, Variadic::String(host_mem_abort));
    inserter->AddArg(esr, Variadic::UnsignedInteger(evt.esr()));
    inserter->AddArg(addr, Variadic::UnsignedInteger(evt.addr()));
  };
  context_->slice_tracker->AddArgs(track_id, category_, slice_name_,
                                   args_inserter);
}

TrackId PkvmHypervisorCpuTracker::GetHypCpuTrackId(uint32_t cpu) {
  base::StackString<255> track_name("pkVM Hypervisor CPU %d", cpu);
  StringId track = context_->storage->InternString(track_name.string_view());
  return context_->track_tracker->InternCpuTrack(track, cpu);
}

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
