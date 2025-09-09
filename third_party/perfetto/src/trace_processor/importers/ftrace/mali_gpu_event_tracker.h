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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_MALI_GPU_EVENT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_MALI_GPU_EVENT_TRACKER_H_

#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class MaliGpuEventTracker {
 public:
  explicit MaliGpuEventTracker(TraceProcessorContext*);
  void ParseMaliGpuEvent(int64_t timestamp, int32_t field_id, uint32_t pid);

 private:
  TraceProcessorContext* context_;
  StringId mali_KCPU_CQS_SET_id_;
  StringId mali_KCPU_CQS_WAIT_id_;
  StringId mali_KCPU_FENCE_SIGNAL_id_;
  StringId mali_KCPU_FENCE_WAIT_id_;
  void ParseMaliKcpuFenceSignal(int64_t timestamp, TrackId track_id);
  void ParseMaliKcpuFenceWaitStart(int64_t timestamp, TrackId track_id);
  void ParseMaliKcpuFenceWaitEnd(int64_t timestamp, TrackId track_id);
  void ParseMaliKcpuCqsSet(int64_t timestamp, TrackId track_id);
  void ParseMaliKcpuCqsWaitStart(int64_t timestamp, TrackId track_id);
  void ParseMaliKcpuCqsWaitEnd(int64_t timestamp, TrackId track_id);
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_MALI_GPU_EVENT_TRACKER_H_
