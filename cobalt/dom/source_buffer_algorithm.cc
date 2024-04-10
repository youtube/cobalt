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

#include "cobalt/dom/source_buffer_algorithm.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/source_buffer_metrics.h"

namespace cobalt {
namespace dom {
namespace {
using ::media::StreamParser;
}  // namespace

SourceBufferAppendAlgorithm::SourceBufferAppendAlgorithm(
    MediaSource* media_source, ::media::ChunkDemuxer* chunk_demuxer,
    const std::string& id, const uint8_t* buffer, size_t size_in_bytes,
    size_t max_append_size_in_bytes, base::TimeDelta append_window_start,
    base::TimeDelta append_window_end, base::TimeDelta timestamp_offset,
    UpdateTimestampOffsetCB&& update_timestamp_offset_cb,
    ScheduleEventCB&& schedule_event_cb, base::OnceClosure&& finalized_cb,
    SourceBufferMetrics* metrics)
    : media_source_(media_source),
      chunk_demuxer_(chunk_demuxer),
      id_(id),
      buffer_(buffer),
      max_append_size_(max_append_size_in_bytes),
      bytes_remaining_(size_in_bytes),
      append_window_start_(append_window_start),
      append_window_end_(append_window_end),
      timestamp_offset_(timestamp_offset),
      update_timestamp_offset_cb_(std::move(update_timestamp_offset_cb)),
      schedule_event_cb_(std::move(schedule_event_cb)),
      finalized_cb_(std::move(finalized_cb)),
      metrics_(metrics) {
  DCHECK(media_source_);
  DCHECK(chunk_demuxer_);
  DCHECK_GT(static_cast<int>(max_append_size_), 0);
  if (bytes_remaining_ > 0) {
    DCHECK(buffer);
  }
  DCHECK(update_timestamp_offset_cb_);
  DCHECK(schedule_event_cb_);
  DCHECK(metrics_);
  TRACE_EVENT1("cobalt::dom", "SourceBufferAppendAlgorithm ctor",
               "bytes_remaining", bytes_remaining_);
}

void SourceBufferAppendAlgorithm::Process(bool* finished) {
  DCHECK(finished);
  DCHECK(!*finished);

  size_t append_size = std::min(bytes_remaining_, max_append_size_);

  TRACE_EVENT1("cobalt::dom", "SourceBufferAppendAlgorithm::Process()",
               "append_size", append_size);

  metrics_->StartTracking(SourceBufferMetricsAction::APPEND_BUFFER);
  succeeded_ = chunk_demuxer_->AppendToParseBuffer(id_, buffer_, append_size);
  StreamParser::ParseStatus result =
      StreamParser::ParseStatus::kSuccessHasMoreData;
  while (succeeded_ &&
         result == StreamParser::ParseStatus::kSuccessHasMoreData) {
    result = chunk_demuxer_->RunSegmentParserLoop(
        id_, append_window_start_, append_window_end_, &timestamp_offset_);
  }
  succeeded_ &= result == StreamParser::ParseStatus::kSuccess;
  update_timestamp_offset_cb_.Run(timestamp_offset_);
  metrics_->EndTracking(SourceBufferMetricsAction::APPEND_BUFFER,
                        succeeded_ ? append_size : 0);

  if (succeeded_) {
    buffer_ += append_size;
    bytes_remaining_ -= append_size;

    if (bytes_remaining_ == 0) {
      *finished = true;
      return;
    }

    return;
  }

  chunk_demuxer_->ResetParserState(id_, append_window_start_,
                                   append_window_end_, &timestamp_offset_);
  update_timestamp_offset_cb_.Run(timestamp_offset_);
  *finished = true;
}

void SourceBufferAppendAlgorithm::Abort() {
  TRACE_EVENT0("cobalt::dom", "SourceBufferAppendAlgorithm::Abort()");
  schedule_event_cb_.Run(base::Tokens::abort());
  schedule_event_cb_.Run(base::Tokens::updateend());
}

void SourceBufferAppendAlgorithm::Finalize() {
  TRACE_EVENT1("cobalt::dom", "SourceBufferAppendAlgorithm::Finalize()",
               "succeeded", succeeded_);

  if (succeeded_) {
    schedule_event_cb_.Run(base::Tokens::update());
    schedule_event_cb_.Run(base::Tokens::updateend());
  } else {
    schedule_event_cb_.Run(base::Tokens::error());
    schedule_event_cb_.Run(base::Tokens::updateend());
    media_source_->EndOfStreamAlgorithm(kMediaSourceEndOfStreamErrorDecode);
  }

  std::move(finalized_cb_).Run();
}

SourceBufferRemoveAlgorithm::SourceBufferRemoveAlgorithm(
    ::media::ChunkDemuxer* chunk_demuxer, const std::string& id,
    base::TimeDelta pending_remove_start, base::TimeDelta pending_remove_end,
    ScheduleEventCB&& schedule_event_cb, base::OnceClosure&& finalized_cb)
    : chunk_demuxer_(chunk_demuxer),
      id_(id),
      pending_remove_start_(pending_remove_start),
      pending_remove_end_(pending_remove_end),
      schedule_event_cb_(std::move(schedule_event_cb)),
      finalized_cb_(std::move(finalized_cb)) {
  TRACE_EVENT0("cobalt::dom", "SourceBufferRemoveAlgorithm ctor");
  DCHECK(chunk_demuxer_);
  DCHECK(schedule_event_cb_);
}

void SourceBufferRemoveAlgorithm::Process(bool* finished) {
  TRACE_EVENT0("cobalt::dom", "SourceBufferRemoveAlgorithm::Process()");
  chunk_demuxer_->Remove(id_, pending_remove_start_, pending_remove_end_);
  *finished = true;
}

void SourceBufferRemoveAlgorithm::Abort() {
  TRACE_EVENT0("cobalt::dom", "SourceBufferRemoveAlgorithm::Abort()");
  // SourceBufferRemoveAlgorithm cannot be cancelled explicitly by the web app.
  // This function will only be called when the SourceBuffer is being removed
  // from the MediaSource and no events should be fired.
}

void SourceBufferRemoveAlgorithm::Finalize() {
  TRACE_EVENT0("cobalt::dom", "SourceBufferRemoveAlgorithm::Finalize()");

  schedule_event_cb_.Run(base::Tokens::update());
  schedule_event_cb_.Run(base::Tokens::updateend());
  std::move(finalized_cb_).Run();
}

}  // namespace dom
}  // namespace cobalt
