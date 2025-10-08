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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_STATSD_MODULE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_STATSD_MODULE_H_

<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/trace_processor/ref_counted.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/trace_processor/importers/common/trace_parser.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/storage/trace_storage.h"
=======
#include <cstdint>
#include <optional>

#include "perfetto/ext/base/flat_hash_map.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/trace_processor/importers/common/async_track_set_tracker.h"
#include "src/trace_processor/importers/common/trace_parser.h"
#include "src/trace_processor/importers/proto/proto_importer_module.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/slice_tables.h"
#include "src/trace_processor/tables/track_tables_py.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/util/descriptors.h"
#include "src/trace_processor/util/proto_to_args_parser.h"

<<<<<<< HEAD
namespace perfetto::trace_processor {
=======
namespace perfetto {
namespace trace_processor {

// Wraps a DescriptorPool and a pointer into that pool. This prevents
// common bugs where moving/changing the pool invalidates the pointer.
class PoolAndDescriptor {
 public:
  PoolAndDescriptor(const uint8_t* data, size_t size, const char* name);
  virtual ~PoolAndDescriptor();

  const DescriptorPool* pool() const { return &pool_; }

  const ProtoDescriptor* descriptor() const { return descriptor_; }

 private:
  PoolAndDescriptor(const PoolAndDescriptor&) = delete;
  PoolAndDescriptor& operator=(const PoolAndDescriptor&) = delete;
  PoolAndDescriptor(PoolAndDescriptor&&) = delete;
  PoolAndDescriptor& operator=(PoolAndDescriptor&&) = delete;

  DescriptorPool pool_;
  const ProtoDescriptor* descriptor_{};
};
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class StatsdModule : public ProtoImporterModule {
 public:
  explicit StatsdModule(TraceProcessorContext* context);

  ~StatsdModule() override;

<<<<<<< HEAD
  ModuleResult TokenizePacket(const protos::pbzero::TracePacket::Decoder&,
                              TraceBlobView* packet,
                              int64_t packet_timestamp,
                              RefPtr<PacketSequenceStateGeneration> state,
                              uint32_t field_id) override;

  void ParseTracePacketData(const protos::pbzero::TracePacket::Decoder& decoder,
=======
  void ParseTracePacketData(const protos::pbzero::TracePacket_Decoder& decoder,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
                            int64_t ts,
                            const TracePacketData&,
                            uint32_t field_id) override;

 private:
  void ParseAtom(int64_t ts, protozero::ConstBytes bytes);
  StringId GetAtomName(uint32_t atom_field_id);
<<<<<<< HEAD
  TrackId InternTrackId();

  TraceProcessorContext* context_;
  base::FlatHashMap<uint32_t, StringId> atom_names_;
  uint32_t descriptor_idx_ = std::numeric_limits<uint32_t>::max();
  util::ProtoToArgsParser args_parser_;
  std::optional<TrackId> track_id_;
};

}  // namespace perfetto::trace_processor
=======
  AsyncTrackSetTracker::TrackSetId InternAsyncTrackSetId();

  TraceProcessorContext* context_;
  base::FlatHashMap<uint32_t, StringId> atom_names_;
  PoolAndDescriptor pool_;
  util::ProtoToArgsParser args_parser_;
  std::optional<AsyncTrackSetTracker::TrackSetId> track_set_id_;
};

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_STATSD_MODULE_H_
