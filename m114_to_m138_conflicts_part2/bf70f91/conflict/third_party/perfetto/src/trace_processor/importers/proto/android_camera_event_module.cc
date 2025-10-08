/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/android_camera_event_module.h"

<<<<<<< HEAD
#include <cinttypes>
#include <cstdint>
#include <utility>

#include "perfetto/ext/base/string_utils.h"
#include "perfetto/protozero/field.h"
#include "perfetto/trace_processor/ref_counted.h"
#include "src/trace_processor/importers/common/parser_types.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_compressor.h"
#include "src/trace_processor/importers/common/tracks.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/android/camera_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto::trace_processor {
=======
#include "perfetto/ext/base/string_utils.h"
#include "protos/perfetto/trace/android/camera_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/trace_processor/importers/common/async_track_set_tracker.h"
#include "src/trace_processor/importers/common/parser_types.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

using perfetto::protos::pbzero::TracePacket;

AndroidCameraEventModule::AndroidCameraEventModule(
    TraceProcessorContext* context)
    : context_(context) {
  RegisterForField(TracePacket::kAndroidCameraFrameEventFieldNumber, context);
}

AndroidCameraEventModule::~AndroidCameraEventModule() = default;

ModuleResult AndroidCameraEventModule::TokenizePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    TraceBlobView* packet,
    int64_t /*packet_timestamp*/,
<<<<<<< HEAD
    RefPtr<PacketSequenceStateGeneration> state,
=======
    PacketSequenceState* state,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    uint32_t field_id) {
  if (field_id != TracePacket::kAndroidCameraFrameEventFieldNumber) {
    return ModuleResult::Ignored();
  }
  const auto android_camera_frame_event =
      protos::pbzero::AndroidCameraFrameEvent::Decoder(
          decoder.android_camera_frame_event());
  context_->sorter->PushTracePacket(
      android_camera_frame_event.request_processing_started_ns(),
<<<<<<< HEAD
      std::move(state), std::move(*packet), context_->machine_id());
=======
      state->current_generation(), std::move(*packet));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  return ModuleResult::Handled();
}

void AndroidCameraEventModule::ParseTracePacketData(
    const TracePacket::Decoder& decoder,
    int64_t /*ts*/,
    const TracePacketData&,
    uint32_t field_id) {
  if (field_id != TracePacket::kAndroidCameraFrameEventFieldNumber) {
    return;
  }
  InsertCameraFrameSlice(decoder.android_camera_frame_event());
}

void AndroidCameraEventModule::InsertCameraFrameSlice(
    protozero::ConstBytes bytes) {
<<<<<<< HEAD
  protos::pbzero::AndroidCameraFrameEvent::Decoder evt(bytes);
  StringId slice_name = context_->storage->InternString(
      base::StackString<32>("Frame %" PRId64, evt.frame_number())
          .string_view());
  int64_t ts = evt.request_processing_started_ns();
  int64_t dur =
      evt.responses_all_sent_ns() - evt.request_processing_started_ns();

  constexpr auto kBlueprint = TrackCompressor::SliceBlueprint(
      "android_camera_event",
      tracks::Dimensions(tracks::UintDimensionBlueprint("android_camera_id")),
      tracks::FnNameBlueprint([](uint32_t camera_id) {
        return base::StackString<32>("Camera %u Frames", camera_id);
      }));

  TrackId track_id = context_->track_compressor->InternScoped(
      kBlueprint, tracks::Dimensions(evt.camera_id()), ts, dur);
=======
  const auto android_camera_frame_event =
      protos::pbzero::AndroidCameraFrameEvent::Decoder(bytes);
  StringId track_name = context_->storage->InternString(
      base::StackString<32>("Camera %d Frames",
                            android_camera_frame_event.camera_id())
          .string_view());
  StringId slice_name = context_->storage->InternString(
      base::StackString<32>("Frame %" PRId64,
                            android_camera_frame_event.frame_number())
          .string_view());
  int64_t ts = android_camera_frame_event.request_processing_started_ns();
  int64_t dur = android_camera_frame_event.responses_all_sent_ns() -
                android_camera_frame_event.request_processing_started_ns();
  auto track_set_id =
      context_->async_track_set_tracker->InternGlobalTrackSet(track_name);
  auto track_id =
      context_->async_track_set_tracker->Scoped(track_set_id, ts, dur);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  context_->slice_tracker->Scoped(ts, track_id, /*category=*/kNullStringId,
                                  slice_name, dur);
}

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
