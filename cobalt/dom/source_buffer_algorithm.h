// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_DOM_SOURCE_BUFFER_ALGORITHM_H_
#define COBALT_DOM_SOURCE_BUFFER_ALGORITHM_H_

#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/source_buffer_metrics.h"
#include "media/filters/chunk_demuxer.h"

namespace cobalt {
namespace dom {

class MediaSource;  // Necessary to avoid circular inclusion.

// Base class of all SourceBuffer algorithms.  Its user should call `Process()`
// multiple times until |finished| is set to true on return, and then call
// `Finalize()` exactly once.  `Abort()` can be called at any time to abort any
// processing.
class SourceBufferAlgorithm {
 public:
  typedef base::RepeatingCallback<void(base::TimeDelta)>
      UpdateTimestampOffsetCB;
  typedef base::RepeatingCallback<void(base_token::Token)> ScheduleEventCB;

  virtual ~SourceBufferAlgorithm() {}

  // Returns false to indicate that the algorithm cannot be explicitly aborted
  // by calling `Abort()` on the Handle.  The algorithm may still be aborted
  // when the SourceBuffer is being removed from the MediaSource.
  virtual bool SupportExplicitAbort() = 0;

  virtual void Process(bool* finished) = 0;
  virtual void Abort() = 0;
  virtual void Finalize() = 0;
};

// Implements the steps 12 to 18 of SourceBuffer stream append loop algorithm as
// specified at
// https://www.w3.org/TR/2016/CR-media-source-20160705/#sourcebuffer-stream-append-loop.
class SourceBufferAppendAlgorithm : public SourceBufferAlgorithm {
 public:
  SourceBufferAppendAlgorithm(
      MediaSource* media_source, ::media::ChunkDemuxer* chunk_demuxer,
      const std::string& id, const uint8_t* buffer, size_t size_in_bytes,
      size_t max_append_size_in_bytes, base::TimeDelta append_window_start,
      base::TimeDelta append_window_end, base::TimeDelta timestamp_offset,
      UpdateTimestampOffsetCB&& update_timestamp_offset_cb,
      ScheduleEventCB&& schedule_event_cb, base::OnceClosure&& finalized_cb,
      SourceBufferMetrics* metrics);

 private:
  bool SupportExplicitAbort() override { return true; }
  void Process(bool* finished) override;
  void Abort() override;
  void Finalize() override;

  MediaSource* const media_source_;
  ::media::ChunkDemuxer* const chunk_demuxer_;
  const std::string id_;
  const uint8_t* buffer_;
  const size_t max_append_size_;
  size_t bytes_remaining_;
  const base::TimeDelta append_window_start_;
  const base::TimeDelta append_window_end_;

  base::TimeDelta timestamp_offset_;
  const UpdateTimestampOffsetCB update_timestamp_offset_cb_;
  const ScheduleEventCB schedule_event_cb_;
  base::OnceClosure finalized_cb_;
  SourceBufferMetrics* const metrics_;

  bool succeeded_ = false;
};

// Implements the steps 6 to 9 of SourceBuffer range removal algorithm as
// specified at
// https://www.w3.org/TR/2016/CR-media-source-20160705/#sourcebuffer-range-removal.
class SourceBufferRemoveAlgorithm : public SourceBufferAlgorithm {
 public:
  SourceBufferRemoveAlgorithm(::media::ChunkDemuxer* chunk_demuxer,
                              const std::string& id,
                              base::TimeDelta pending_remove_start,
                              base::TimeDelta pending_remove_end,
                              ScheduleEventCB&& schedule_event_cb,
                              base::OnceClosure&& finalized_cb);

 private:
  bool SupportExplicitAbort() override { return false; }
  void Process(bool* finished) override;
  void Abort() override;
  void Finalize() override;

  ::media::ChunkDemuxer* const chunk_demuxer_;
  const std::string id_;
  const base::TimeDelta pending_remove_start_;
  const base::TimeDelta pending_remove_end_;
  const ScheduleEventCB schedule_event_cb_;
  base::OnceClosure finalized_cb_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SOURCE_BUFFER_ALGORITHM_H_
