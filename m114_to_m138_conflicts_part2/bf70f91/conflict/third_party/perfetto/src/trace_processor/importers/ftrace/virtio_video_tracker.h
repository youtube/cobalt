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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_VIRTIO_VIDEO_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_VIRTIO_VIDEO_TRACKER_H_

#include <cstdint>

#include "perfetto/ext/base/flat_hash_map.h"
<<<<<<< HEAD
#include "perfetto/protozero/field.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/track_compressor.h"
=======

#include "perfetto/protozero/field.h"
#include "protos/perfetto/trace/ftrace/virtio_video.pbzero.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/async_track_set_tracker.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/destructible.h"
#include "src/trace_processor/types/trace_processor_context.h"

<<<<<<< HEAD
#include "protos/perfetto/trace/ftrace/virtio_video.pbzero.h"

namespace perfetto::trace_processor {
=======
namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessorContext;

class VirtioVideoTracker : public Destructible {
 public:
  // Declared public for testing only.
  explicit VirtioVideoTracker(TraceProcessorContext*);
  VirtioVideoTracker(const VirtioVideoTracker&) = delete;
  VirtioVideoTracker& operator=(const VirtioVideoTracker&) = delete;
  ~VirtioVideoTracker() override;

  static VirtioVideoTracker* GetOrCreate(TraceProcessorContext* context) {
    if (!context->virtio_video_tracker) {
      context->virtio_video_tracker.reset(new VirtioVideoTracker(context));
    }
    return static_cast<VirtioVideoTracker*>(
        context->virtio_video_tracker.get());
  }

  void ParseVirtioVideoEvent(uint64_t fld_id,
                             int64_t timestamp,
                             const protozero::ConstBytes&);

 private:
  struct FieldsStringIds {
<<<<<<< HEAD
    explicit FieldsStringIds(TraceStorage& storage);
=======
    FieldsStringIds(TraceStorage& storage);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    StringId stream_id;
    StringId resource_id;
    StringId queue_type;
    StringId data_size0;
    StringId data_size1;
    StringId data_size2;
    StringId data_size3;
    StringId timestamp;
  };

<<<<<<< HEAD
=======
  AsyncTrackSetTracker::TrackSetId InternOrCreateBufferTrack(
      int32_t stream_id,
      uint32_t queue_type);

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  void AddCommandSlice(int64_t timestamp,
                       uint32_t stream_id,
                       uint64_t type,
                       bool response);

  void AddCommandSliceArgs(
      protos::pbzero::VirtioVideoResourceQueueDoneFtraceEvent::Decoder*,
      ArgsTracker::BoundInserter*);

  TraceProcessorContext* const context_;

  StringId unknown_id_;
  StringId input_queue_id_;
  StringId output_queue_id_;

  FieldsStringIds fields_string_ids_;
  base::FlatHashMap<uint64_t, StringId> command_names_;
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_VIRTIO_VIDEO_TRACKER_H_
