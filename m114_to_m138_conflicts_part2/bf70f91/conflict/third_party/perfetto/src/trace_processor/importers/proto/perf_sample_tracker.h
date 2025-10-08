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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_

<<<<<<< HEAD
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/profiler_tables_py.h"

namespace perfetto {

namespace protos::pbzero {
class TracePacketDefaults_Decoder;
}  // namespace protos::pbzero

=======
#include <stdint.h>

#include <unordered_map>

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class TracePacketDefaults_Decoder;
}  // namespace pbzero
}  // namespace protos
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
namespace trace_processor {
class TraceProcessorContext;

class PerfSampleTracker {
 public:
  struct SamplingStreamInfo {
<<<<<<< HEAD
    tables::PerfSessionTable::Id perf_session_id;
    TrackId timebase_track_id = kInvalidTrackId;
    std::vector<TrackId> follower_track_ids;

    SamplingStreamInfo(tables::PerfSessionTable::Id _perf_session_id,
                       TrackId _timebase_track_id,
                       std::vector<TrackId> _follower_track_ids)
        : perf_session_id(_perf_session_id),
          timebase_track_id(_timebase_track_id),
          follower_track_ids(std::move(_follower_track_ids)) {}
  };

  explicit PerfSampleTracker(TraceProcessorContext* context);
=======
    uint32_t perf_session_id = 0;
    TrackId timebase_track_id = kInvalidTrackId;

    SamplingStreamInfo(uint32_t _perf_session_id, TrackId _timebase_track_id)
        : perf_session_id(_perf_session_id),
          timebase_track_id(_timebase_track_id) {}
  };

  explicit PerfSampleTracker(TraceProcessorContext* context)
      : context_(context) {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  SamplingStreamInfo GetSamplingStreamInfo(
      uint32_t seq_id,
      uint32_t cpu,
      protos::pbzero::TracePacketDefaults_Decoder* nullable_defaults);

 private:
  struct CpuSequenceState {
    TrackId timebase_track_id = kInvalidTrackId;
<<<<<<< HEAD
    std::vector<TrackId> follower_track_ids;

    CpuSequenceState(TrackId _timebase_track_id,
                     std::vector<TrackId> _follower_track_ids)
        : timebase_track_id(_timebase_track_id),
          follower_track_ids(std::move(_follower_track_ids)) {}
  };

  struct SequenceState {
    tables::PerfSessionTable::Id perf_session_id;
    std::unordered_map<uint32_t, CpuSequenceState> per_cpu;

    explicit SequenceState(tables::PerfSessionTable::Id _perf_session_id)
        : perf_session_id(_perf_session_id) {}
  };

  tables::PerfSessionTable::Id CreatePerfSession();

  std::unordered_map<uint32_t, SequenceState> seq_state_;
  const StringId is_timebase_id_;
=======

    CpuSequenceState(TrackId _timebase_track_id)
        : timebase_track_id(_timebase_track_id) {}
  };

  struct SequenceState {
    uint32_t perf_session_id = 0;
    std::unordered_map<uint32_t, CpuSequenceState> per_cpu;

    SequenceState(uint32_t _perf_session_id)
        : perf_session_id(_perf_session_id) {}
  };

  std::unordered_map<uint32_t, SequenceState> seq_state_;
  uint32_t next_perf_session_id_ = 0;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  TraceProcessorContext* const context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_
