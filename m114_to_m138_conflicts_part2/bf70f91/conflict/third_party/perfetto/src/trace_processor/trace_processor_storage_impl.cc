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

#include "src/trace_processor/trace_processor_storage_impl.h"

<<<<<<< HEAD
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "perfetto/base/status.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/uuid.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/forwarding_trace_parser.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/clock_converter.h"  // IWYU pragma: keep
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/metadata_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/stack_profile_tracker.h"
#include "src/trace_processor/importers/common/trace_file_tracker.h"
#include "src/trace_processor/importers/proto/default_modules.h"
#include "src/trace_processor/importers/proto/packet_analyzer.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/proto_trace_parser_impl.h"
#include "src/trace_processor/importers/proto/proto_trace_reader.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_reader_registry.h"
#include "src/trace_processor/types/variadic.h"
#include "src/trace_processor/util/descriptors.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_processor/util/trace_type.h"

namespace perfetto::trace_processor {

TraceProcessorStorageImpl::TraceProcessorStorageImpl(const Config& cfg)
    : context_({cfg, std::make_shared<TraceStorage>(cfg)}) {
  context_.reader_registry->RegisterTraceReader<ProtoTraceReader>(
      kProtoTraceType);
  context_.reader_registry->RegisterTraceReader<ProtoTraceReader>(
      kSymbolsTraceType);
  context_.proto_trace_parser =
      std::make_unique<ProtoTraceParserImpl>(&context_);
=======
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/uuid.h"
#include "src/trace_processor/forwarding_trace_parser.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/args_translation_table.h"
#include "src/trace_processor/importers/common/async_track_set_tracker.h"
#include "src/trace_processor/importers/common/clock_converter.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/flow_tracker.h"
#include "src/trace_processor/importers/common/metadata_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/slice_translation_table.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/chrome_track_event.descriptor.h"
#include "src/trace_processor/importers/proto/default_modules.h"
#include "src/trace_processor/importers/proto/heap_profile_tracker.h"
#include "src/trace_processor/importers/proto/packet_analyzer.h"
#include "src/trace_processor/importers/proto/perf_sample_tracker.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/importers/proto/proto_trace_reader.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/importers/proto/track_event.descriptor.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/util/descriptors.h"

namespace perfetto {
namespace trace_processor {

TraceProcessorStorageImpl::TraceProcessorStorageImpl(const Config& cfg) {
  context_.config = cfg;

  context_.storage.reset(new TraceStorage(context_.config));
  context_.track_tracker.reset(new TrackTracker(&context_));
  context_.async_track_set_tracker.reset(new AsyncTrackSetTracker(&context_));
  context_.args_tracker.reset(new ArgsTracker(&context_));
  context_.args_translation_table.reset(
      new ArgsTranslationTable(context_.storage.get()));
  context_.slice_tracker.reset(new SliceTracker(&context_));
  context_.slice_translation_table.reset(
      new SliceTranslationTable(context_.storage.get()));
  context_.flow_tracker.reset(new FlowTracker(&context_));
  context_.event_tracker.reset(new EventTracker(&context_));
  context_.process_tracker.reset(new ProcessTracker(&context_));
  context_.clock_tracker.reset(new ClockTracker(&context_));
  context_.clock_converter.reset(new ClockConverter(&context_));
  context_.heap_profile_tracker.reset(new HeapProfileTracker(&context_));
  context_.perf_sample_tracker.reset(new PerfSampleTracker(&context_));
  context_.global_stack_profile_tracker.reset(new GlobalStackProfileTracker());
  context_.metadata_tracker.reset(new MetadataTracker(context_.storage.get()));
  context_.global_args_tracker.reset(
      new GlobalArgsTracker(context_.storage.get()));
  {
    context_.descriptor_pool_.reset(new DescriptorPool());
    auto status = context_.descriptor_pool_->AddFromFileDescriptorSet(
        kTrackEventDescriptor.data(), kTrackEventDescriptor.size());

    PERFETTO_DCHECK(status.ok());

    status = context_.descriptor_pool_->AddFromFileDescriptorSet(
        kChromeTrackEventDescriptor.data(), kChromeTrackEventDescriptor.size());

    PERFETTO_DCHECK(status.ok());
  }

  context_.slice_tracker->SetOnSliceBeginCallback(
      [this](TrackId track_id, SliceId slice_id) {
        context_.flow_tracker->ClosePendingEventsOnTrack(track_id, slice_id);
      });

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  RegisterDefaultModules(&context_);
}

TraceProcessorStorageImpl::~TraceProcessorStorageImpl() {}

<<<<<<< HEAD
base::Status TraceProcessorStorageImpl::Parse(TraceBlobView blob) {
  if (blob.size() == 0)
    return base::OkStatus();
  if (unrecoverable_parse_error_)
    return base::ErrStatus(
        "Failed unrecoverably while parsing in a previous Parse call");
  if (eof_) {
    return base::ErrStatus("Parse() called after NotifyEndOfFile()");
  }

  if (!parser_) {
    auto parser = std::make_unique<ForwardingTraceParser>(
        &context_, context_.trace_file_tracker->AddFile());
    parser_ = parser.get();
    context_.chunk_readers.push_back(std::move(parser));
  }
=======
util::Status TraceProcessorStorageImpl::Parse(TraceBlobView blob) {
  if (blob.size() == 0)
    return util::OkStatus();
  if (unrecoverable_parse_error_)
    return util::ErrStatus(
        "Failed unrecoverably while parsing in a previous Parse call");
  if (!context_.chunk_reader)
    context_.chunk_reader.reset(new ForwardingTraceParser(&context_));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  auto scoped_trace = context_.storage->TraceExecutionTimeIntoStats(
      stats::parse_trace_duration_ns);

  if (hash_input_size_remaining_ > 0 && !context_.uuid_found_in_trace) {
    const size_t hash_size = std::min(hash_input_size_remaining_, blob.size());
    hash_input_size_remaining_ -= hash_size;

    trace_hash_.Update(reinterpret_cast<const char*>(blob.data()), hash_size);
    base::Uuid uuid(static_cast<int64_t>(trace_hash_.digest()), 0);
    const StringId id_for_uuid =
        context_.storage->InternString(base::StringView(uuid.ToPrettyString()));
    context_.metadata_tracker->SetMetadata(metadata::trace_uuid,
                                           Variadic::String(id_for_uuid));
  }

<<<<<<< HEAD
  base::Status status = parser_->Parse(std::move(blob));
=======
  util::Status status = context_.chunk_reader->Parse(std::move(blob));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  unrecoverable_parse_error_ |= !status.ok();
  return status;
}

void TraceProcessorStorageImpl::Flush() {
  if (unrecoverable_parse_error_)
    return;

  if (context_.sorter)
    context_.sorter->ExtractEventsForced();
<<<<<<< HEAD
  context_.args_tracker->Flush();
}

base::Status TraceProcessorStorageImpl::NotifyEndOfFile() {
  if (!parser_) {
    return base::OkStatus();
  }
  if (unrecoverable_parse_error_) {
    return base::ErrStatus("Unrecoverable parsing error already occurred");
  }
  eof_ = true;
  Flush();
  RETURN_IF_ERROR(parser_->NotifyEndOfFile());
  // NotifyEndOfFile might have pushed packets to the sorter.
  Flush();
=======
}

void TraceProcessorStorageImpl::NotifyEndOfFile() {
  if (unrecoverable_parse_error_ || !context_.chunk_reader)
    return;
  Flush();
  context_.chunk_reader->NotifyEndOfFile();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  for (std::unique_ptr<ProtoImporterModule>& module : context_.modules) {
    module->NotifyEndOfFile();
  }
  if (context_.content_analyzer) {
    PacketAnalyzer::Get(&context_)->NotifyEndOfFile();
  }
<<<<<<< HEAD

  context_.event_tracker->FlushPendingEvents();
  context_.slice_tracker->FlushPendingSlices();
  context_.args_tracker->Flush();
  context_.process_tracker->NotifyEndOfFile();
  return base::OkStatus();
=======
  context_.event_tracker->FlushPendingEvents();
  context_.slice_tracker->FlushPendingSlices();
  context_.heap_profile_tracker->NotifyEndOfFile();
  context_.args_tracker->Flush();
  context_.process_tracker->NotifyEndOfFile();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

void TraceProcessorStorageImpl::DestroyContext() {
  TraceProcessorContext context;
  context.storage = std::move(context_.storage);
<<<<<<< HEAD

  // TODO(b/309623584): Decouple from storage and remove from here. This
  // function should only move storage and delete everything else.
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  context.heap_graph_tracker = std::move(context_.heap_graph_tracker);
  context.clock_converter = std::move(context_.clock_converter);
  // "to_ftrace" textual converter of the "raw" table requires remembering the
  // kernel version (inside system_info_tracker) to know how to textualise
  // sched_switch.prev_state bitflags.
  context.system_info_tracker = std::move(context_.system_info_tracker);
<<<<<<< HEAD
  // "__intrinsic_winscope_proto_to_args_with_defaults" requires proto
  // descriptors.
  context.descriptor_pool_ = std::move(context_.descriptor_pool_);

  context_ = std::move(context);

  // This is now a dangling pointer, reset it.
  parser_ = nullptr;

  // TODO(chinglinyu): also need to destroy secondary contextes.
}

}  // namespace perfetto::trace_processor
=======

  context_ = std::move(context);
}

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
