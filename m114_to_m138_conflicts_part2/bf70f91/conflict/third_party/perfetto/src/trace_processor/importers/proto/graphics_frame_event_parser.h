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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_FRAME_EVENT_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_FRAME_EVENT_PARSER_H_

<<<<<<< HEAD
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/android/graphics_frame_event.pbzero.h"
#include "src/trace_processor/tables/slice_tables_py.h"

namespace perfetto::trace_processor {
=======
#include <optional>
#include <vector>

#include "perfetto/ext/base/string_writer.h"
#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/proto/proto_incremental_state.h"
#include "src/trace_processor/importers/proto/vulkan_memory_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"

#include "protos/perfetto/trace/android/graphics_frame_event.pbzero.h"

namespace perfetto {

namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessorContext;

// Class for parsing graphics frame related events.
class GraphicsFrameEventParser {
 public:
  using ConstBytes = protozero::ConstBytes;
  explicit GraphicsFrameEventParser(TraceProcessorContext*);

  void ParseGraphicsFrameEvent(int64_t timestamp, ConstBytes);

 private:
<<<<<<< HEAD
  using SliceRowNumber = tables::SliceTable::RowNumber;
  struct BufferEvent {
    int64_t acquire_ts = 0;
    int64_t queue_ts = 0;
    int64_t latch_ts = 0;
    bool is_most_recent_dequeue_ = false;
  };
  struct DequeueInfo {
    tables::SliceTable::RowNumber slice_row;
    int64_t timestamp;
  };
  struct QueueInfo {
    TrackId track;
  };
  struct LatchInfo {
    TrackId track;
  };
  struct PhaseEvent {
    std::variant<std::monostate, DequeueInfo, QueueInfo, LatchInfo>
        most_recent_event;
    std::optional<int64_t> last_acquire_ts;
  };

  using GraphicsFrameEventDecoder =
      protos::pbzero::GraphicsFrameEvent_BufferEvent_Decoder;
  using GraphicsFrameEvent = protos::pbzero::GraphicsFrameEvent;

  void CreateBufferEvent(int64_t timestamp,
                         const GraphicsFrameEventDecoder&,
                         StringId layer_name_id,
                         StringId event_key);
  void CreatePhaseEvent(int64_t timestamp,
                        const GraphicsFrameEventDecoder&,
                        StringId layer_name_id,
                        StringId event_key);

  std::optional<SliceRowNumber> InsertPhaseSlice(
      int64_t timestamp,
      const GraphicsFrameEventDecoder&,
      TrackId track_id,
      StringId layer_name_id);

  TraceProcessorContext* const context_;
  const StringId unknown_event_name_id_;
  const StringId no_layer_name_name_id_;
  const StringId layer_name_key_id_;
  const StringId queue_lost_message_id_;
  const StringId frame_number_id_;
  const StringId queue_to_acquire_time_id_;
  const StringId acquire_to_latch_time_id_;
  const StringId latch_to_present_time_id_;
  std::array<StringId, 14> event_type_name_ids_;

  // Map of (buffer ID + layer name) -> BufferEvent
  base::FlatHashMap<StringId, BufferEvent> buffer_event_map_;

  // Maps of (buffer id + layer name) -> track id
  base::FlatHashMap<StringId, PhaseEvent> phase_event_map_;

  // Map of layer name -> track id
  base::FlatHashMap<StringId, TrackId> display_map_;
};
}  // namespace perfetto::trace_processor
=======
  using GraphicsFrameEventDecoder =
      protos::pbzero::GraphicsFrameEvent_BufferEvent_Decoder;
  using GraphicsFrameEvent = protos::pbzero::GraphicsFrameEvent;
  bool CreateBufferEvent(int64_t timestamp, GraphicsFrameEventDecoder& event);
  void CreatePhaseEvent(int64_t timestamp, GraphicsFrameEventDecoder& event);
  // Invalidate a phase slice that has one of the events missing
  void InvalidatePhaseEvent(int64_t timestamp,
                            TrackId track_id,
                            bool reset_name = false);

  TraceProcessorContext* const context_;
  const StringId graphics_event_scope_id_;
  const StringId unknown_event_name_id_;
  const StringId no_layer_name_name_id_;
  const StringId layer_name_key_id_;
  std::array<StringId, 14> event_type_name_ids_;
  const StringId queue_lost_message_id_;
  // Map of (buffer ID + layer name) -> slice id of the dequeue event
  std::unordered_map<StringId, SliceId> dequeue_slice_ids_;

  // Row indices of frame stats table. Used to populate the slice_id after
  // inserting the rows.
  std::vector<uint32_t> graphics_frame_stats_idx_;
  // Map of (buffer ID + layer name)
  //    -> (Map of GraphicsFrameEvent -> ts of that event)
  std::unordered_map<StringId, std::unordered_map<uint64_t, int64_t>>
      graphics_frame_stats_map_;

  // Maps of (buffer id + layer name) -> track id
  std::unordered_map<StringId, TrackId> dequeue_map_;
  std::unordered_map<StringId, TrackId> queue_map_;
  std::unordered_map<StringId, TrackId> latch_map_;
  // Map of layer name -> track id
  std::unordered_map<StringId, TrackId> display_map_;

  // Maps of (buffer id + layer name) -> timestamp
  std::unordered_map<StringId, int64_t> last_dequeued_;
  std::unordered_map<StringId, int64_t> last_acquired_;
};
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_GRAPHICS_FRAME_EVENT_PARSER_H_
