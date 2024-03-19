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

#include <stdint.h>

#include <unordered_map>

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class TracePacketDefaults_Decoder;
}  // namespace pbzero
}  // namespace protos
namespace trace_processor {
class TraceProcessorContext;

class PerfSampleTracker {
 public:
  struct SamplingStreamInfo {
    uint32_t perf_session_id = 0;
    TrackId timebase_track_id = kInvalidTrackId;

    SamplingStreamInfo(uint32_t _perf_session_id, TrackId _timebase_track_id)
        : perf_session_id(_perf_session_id),
          timebase_track_id(_timebase_track_id) {}
  };

  explicit PerfSampleTracker(TraceProcessorContext* context)
      : context_(context) {}

  SamplingStreamInfo GetSamplingStreamInfo(
      uint32_t seq_id,
      uint32_t cpu,
      protos::pbzero::TracePacketDefaults_Decoder* nullable_defaults);

 private:
  struct CpuSequenceState {
    TrackId timebase_track_id = kInvalidTrackId;

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

  TraceProcessorContext* const context_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PERF_SAMPLE_TRACKER_H_
