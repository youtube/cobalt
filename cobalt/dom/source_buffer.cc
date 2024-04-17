/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/source_buffer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/media_settings.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/source_buffer_metrics.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/web_settings.h"
#include "media/base/ranges.h"
#include "media/base/timestamp_constants.h"

namespace cobalt {
namespace dom {

namespace {

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!std::isnan(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  if (time == std::numeric_limits<double>::infinity()) {
    return ::media::kInfiniteDuration;
  }

  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_time = base::TimeDelta::FromInternalValue(
      std::numeric_limits<int64_t>::max() - 1);
  double max_time_in_seconds = max_time.InSecondsF();

  if (time >= max_time_in_seconds) {
    return max_time;
  }

  return base::TimeDelta::FromSecondsD(time);
}

const MediaSettings& GetMediaSettings(web::EnvironmentSettings* settings) {
  DCHECK(settings);
  DCHECK(settings->context());
  DCHECK(settings->context()->web_settings());

  const auto& web_settings = settings->context()->web_settings();
  return web_settings->media_settings();
}

// The return value will be used in `SourceBuffer::EvictCodedFrames()` to allow
// it to evict extra data from the SourceBuffer, so it can reduce the overall
// memory used by the underlying Demuxer implementation.
// The default value is 0, i.e. do not evict extra bytes.
size_t GetEvictExtraInBytes(web::EnvironmentSettings* settings) {
  const MediaSettings& media_settings = GetMediaSettings(settings);

  int bytes = media_settings.GetSourceBufferEvictExtraInBytes().value_or(0);
  DCHECK_GE(bytes, 0);

  return std::max<int>(bytes, 0);
}

size_t GetMaxAppendSizeInBytes(web::EnvironmentSettings* settings,
                               size_t default_value) {
  const MediaSettings& media_settings = GetMediaSettings(settings);

  int bytes = media_settings.GetMaxSourceBufferAppendSizeInBytes().value_or(
      default_value);
  DCHECK_GT(bytes, 0);

  return bytes;
}

bool IsAvoidCopyingArrayBufferEnabled(web::EnvironmentSettings* settings) {
  const MediaSettings& media_settings = GetMediaSettings(settings);
  return media_settings.IsAvoidCopyingArrayBufferEnabled().value_or(false);
}

}  // namespace

SourceBuffer::OnInitSegmentReceivedHelper::OnInitSegmentReceivedHelper(
    SourceBuffer* source_buffer)
    : source_buffer_(source_buffer) {
  DCHECK(source_buffer_);
}

void SourceBuffer::OnInitSegmentReceivedHelper::Detach() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  source_buffer_ = nullptr;
}

void SourceBuffer::OnInitSegmentReceivedHelper::TryToRunOnInitSegmentReceived(
    std::unique_ptr<MediaTracks> tracks) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&OnInitSegmentReceivedHelper::TryToRunOnInitSegmentReceived,
                   this, base::Passed(&tracks)));
    return;
  }

  if (source_buffer_) {
    source_buffer_->OnInitSegmentReceived(std::move(tracks));
  }
}

SourceBuffer::SourceBuffer(script::EnvironmentSettings* settings,
                           const std::string& id, MediaSource* media_source,
                           ChunkDemuxer* chunk_demuxer, EventQueue* event_queue)
    : web::EventTarget(settings),
      on_init_segment_received_helper_(new OnInitSegmentReceivedHelper(this)),
      id_(id),
      evict_extra_in_bytes_(GetEvictExtraInBytes(environment_settings())),
      max_append_buffer_size_(GetMaxAppendSizeInBytes(
          environment_settings(), kDefaultMaxAppendBufferSize)),
      media_source_(media_source),
      chunk_demuxer_(chunk_demuxer),
      event_queue_(event_queue),
      audio_tracks_(
          new AudioTrackList(settings, media_source->GetMediaElement())),
      video_tracks_(
          new VideoTrackList(settings, media_source->GetMediaElement())),
      metrics_(!media_source_->MediaElementHasMaxVideoCapabilities()) {
  DCHECK(!id_.empty());
  DCHECK(media_source_);
  DCHECK(chunk_demuxer);
  DCHECK(event_queue);

  LOG(INFO) << "Evict extra in bytes is set to " << evict_extra_in_bytes_;
  LOG(INFO) << "Max append size in bytes is set to " << max_append_buffer_size_;

  chunk_demuxer_->SetTracksWatcher(
      id_,
      base::Bind(&OnInitSegmentReceivedHelper::TryToRunOnInitSegmentReceived,
                 on_init_segment_received_helper_));
  chunk_demuxer_->SetParseWarningCallback(
      id, base::BindRepeating([](::media::SourceBufferParseWarning warning) {
        LOG(WARNING) << "Encountered SourceBufferParseWarning "
                     << static_cast<int>(warning);
      }));
}

void SourceBuffer::set_mode(SourceBufferAppendMode mode,
                            script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  media_source_->OpenIfInEndedState();

  if (chunk_demuxer_->IsParsingMediaSegment(id_)) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  chunk_demuxer_->SetSequenceMode(id_, mode == kSourceBufferAppendModeSequence);

  mode_ = mode;
}

scoped_refptr<TimeRanges> SourceBuffer::buffered(
    script::ExceptionState* exception_state) const {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return NULL;
  }

  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  ::media::Ranges<base::TimeDelta> ranges =
      chunk_demuxer_->GetBufferedRanges(id_);
  for (size_t i = 0; i < ranges.size(); i++) {
    time_ranges->Add(ranges.start(i).InSecondsF(), ranges.end(i).InSecondsF());
  }
  return time_ranges;
}

double SourceBuffer::timestamp_offset(
    script::ExceptionState* exception_state) const {
  starboard::ScopedLock scoped_lock(timestamp_offset_mutex_);
  return timestamp_offset_;
}

void SourceBuffer::set_timestamp_offset(
    double offset, script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  media_source_->OpenIfInEndedState();

  if (chunk_demuxer_->IsParsingMediaSegment(id_)) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  // We don't have to acquire |timestamp_offset_mutex_|, as no algorithms are
  // running asynchronously at this moment, which is guaranteed by the check of
  // updating() above.
  timestamp_offset_ = offset;

  chunk_demuxer_->SetGroupStartTimestampIfInSequenceMode(
      id_, DoubleToTimeDelta(timestamp_offset_));
}

void SourceBuffer::set_append_window_start(
    double start, script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (start < 0 || start >= append_window_end_) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }

  append_window_start_ = start;
}

void SourceBuffer::set_append_window_end(
    double end, script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (std::isnan(end)) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }
  // 4. If the new value is less than or equal to appendWindowStart then throw a
  //    TypeError exception and abort these steps.
  if (end <= append_window_start_) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }

  append_window_end_ = end;
}

void SourceBuffer::AppendBuffer(const script::Handle<ArrayBuffer>& data,
                                script::ExceptionState* exception_state) {
  TRACE_EVENT1("cobalt::dom", "SourceBuffer::AppendBuffer()", "size",
               data->ByteLength());

  is_avoid_copying_array_buffer_enabled_ =
      IsAvoidCopyingArrayBufferEnabled(environment_settings());

  DCHECK(array_buffer_in_use_.IsEmpty());
  DCHECK(array_buffer_view_in_use_.IsEmpty());
  if (is_avoid_copying_array_buffer_enabled_) {
    array_buffer_in_use_ = data;
  }

  AppendBufferInternal(static_cast<const unsigned char*>(data->Data()),
                       data->ByteLength(), exception_state);
}

void SourceBuffer::AppendBuffer(const script::Handle<ArrayBufferView>& data,
                                script::ExceptionState* exception_state) {
  TRACE_EVENT1("cobalt::dom", "SourceBuffer::AppendBuffer()", "size",
               data->ByteLength());

  is_avoid_copying_array_buffer_enabled_ =
      IsAvoidCopyingArrayBufferEnabled(environment_settings());

  DCHECK(array_buffer_in_use_.IsEmpty());
  DCHECK(array_buffer_view_in_use_.IsEmpty());
  if (is_avoid_copying_array_buffer_enabled_) {
    array_buffer_view_in_use_ = data;
  }

  AppendBufferInternal(static_cast<const unsigned char*>(data->RawData()),
                       data->ByteLength(), exception_state);
}

void SourceBuffer::Abort(script::ExceptionState* exception_state) {
  TRACE_EVENT0("cobalt::dom", "SourceBuffer::Abort()");

  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (!media_source_->IsOpen()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (active_algorithm_handle_) {
    if (!active_algorithm_handle_->algorithm()->SupportExplicitAbort()) {
      web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                               exception_state);
      return;
    }
    active_algorithm_handle_->Abort();
    active_algorithm_handle_ = nullptr;
  }

  base::TimeDelta timestamp_offset = DoubleToTimeDelta(timestamp_offset_);
  chunk_demuxer_->ResetParserState(id_, DoubleToTimeDelta(append_window_start_),
                                   DoubleToTimeDelta(append_window_end_),
                                   &timestamp_offset);
  UpdateTimestampOffset(timestamp_offset);

  set_append_window_start(0, exception_state);
  set_append_window_end(std::numeric_limits<double>::infinity(),
                        exception_state);
}

void SourceBuffer::Remove(double start, double end,
                          script::ExceptionState* exception_state) {
  TRACE_EVENT2("cobalt::dom", "SourceBuffer::Remove()", "start", start, "end",
               end);
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (start < 0 || std::isnan(media_source_->duration(NULL)) ||
      start > media_source_->duration(NULL)) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }

  if (end <= start || std::isnan(end)) {
    exception_state->SetSimpleException(script::kSimpleTypeError);
    return;
  }

  media_source_->OpenIfInEndedState();

  ScheduleEvent(base::Tokens::updatestart());

  DCHECK_GE(start, 0);
  DCHECK_LT(start, end);

  std::unique_ptr<SourceBufferAlgorithm> algorithm(
      new SourceBufferRemoveAlgorithm(
          chunk_demuxer_, id_, DoubleToTimeDelta(start), DoubleToTimeDelta(end),
          base::Bind(&SourceBuffer::ScheduleEvent, base::Unretained(this)),
          base::Bind(&SourceBuffer::OnAlgorithmFinalized,
                     base::Unretained(this))));
  auto algorithm_runner =
      media_source_->GetAlgorithmRunner(std::numeric_limits<int>::max());
  active_algorithm_handle_ =
      algorithm_runner->CreateHandle(std::move(algorithm));
  algorithm_runner->Start(active_algorithm_handle_);
}

void SourceBuffer::set_track_defaults(
    const scoped_refptr<TrackDefaultList>& track_defaults,
    script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  track_defaults_ = track_defaults;
}

double SourceBuffer::write_head(script::ExceptionState* exception_state) const {
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return 0.0;
  }

  DCHECK(chunk_demuxer_);
  return chunk_demuxer_->GetWriteHead(id_).InSecondsF();
}

void SourceBuffer::OnRemovedFromMediaSource() {
  if (media_source_ == NULL) {
    return;
  }

  DCHECK(on_init_segment_received_helper_);
  on_init_segment_received_helper_->Detach();

  if (active_algorithm_handle_) {
    active_algorithm_handle_->Abort();
    active_algorithm_handle_ = nullptr;
  }

  DCHECK(media_source_);

  // TODO: Implement track support.
  // if (media_source_->GetMediaElement()->audio_tracks().length() > 0 ||
  //     media_source_->GetMediaElement()->audio_tracks().length() > 0) {
  //   RemoveMediaTracks();
  // }

  // TODO: Determine if the source buffer contains an audio or video stream,
  //       maybe by implementing track support and get from the type of the
  //       track, and print the steam type along with the metrics.
  metrics_.PrintCurrentMetricsAndUpdateAccumulatedMetrics();

  chunk_demuxer_->RemoveId(id_);
  if (chunk_demuxer_->GetAllStreams().empty()) {
    metrics_.PrintAccumulatedMetrics();
  }

  chunk_demuxer_ = NULL;
  media_source_ = NULL;
  event_queue_ = NULL;

  pending_append_data_.reset();
  pending_append_data_capacity_ = 0;
  array_buffer_in_use_ = script::Handle<ArrayBuffer>();
  array_buffer_view_in_use_ = script::Handle<ArrayBufferView>();
}

double SourceBuffer::GetHighestPresentationTimestamp() const {
  DCHECK(media_source_ != NULL);

  return chunk_demuxer_->GetHighestPresentationTimestamp(id_).InSecondsF();
}

void SourceBuffer::TraceMembers(script::Tracer* tracer) {
  web::EventTarget::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(media_source_);
  tracer->Trace(track_defaults_);
  tracer->Trace(audio_tracks_);
  tracer->Trace(video_tracks_);
}

size_t SourceBuffer::memory_limit(
    script::ExceptionState* exception_state) const {
  if (!chunk_demuxer_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return 0;
  }

  size_t memory_limit = chunk_demuxer_->GetSourceBufferStreamMemoryLimit(id_);

  if (memory_limit == 0) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
  }
  return memory_limit;
}

void SourceBuffer::set_memory_limit(size_t memory_limit,
                                    script::ExceptionState* exception_state) {
  if (!chunk_demuxer_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (memory_limit == 0) {
    web::DOMException::Raise(web::DOMException::kNotSupportedErr,
                             exception_state);
    return;
  }
  chunk_demuxer_->SetSourceBufferStreamMemoryLimit(id_, memory_limit);
}

void SourceBuffer::OnInitSegmentReceived(std::unique_ptr<MediaTracks> tracks) {
  if (!first_initialization_segment_received_) {
    media_source_->SetSourceBufferActive(this, true);
    first_initialization_segment_received_ = true;
  }

  // TODO: Implement track support.
}

void SourceBuffer::ScheduleEvent(base_token::Token event_name) {
  scoped_refptr<web::Event> event = new web::Event(event_name);
  event->set_target(this);
  event_queue_->Enqueue(event);
}

bool SourceBuffer::PrepareAppend(size_t new_data_size,
                                 script::ExceptionState* exception_state) {
  TRACE_EVENT1("cobalt::dom", "SourceBuffer::PrepareAppend()", "new_data_size",
               new_data_size);
  if (media_source_ == NULL) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return false;
  }
  if (updating()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return false;
  }

  DCHECK(media_source_->GetMediaElement());
  if (media_source_->GetMediaElement()->error()) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return false;
  }

  metrics_.StartTracking(SourceBufferMetricsAction::PREPARE_APPEND);
  media_source_->OpenIfInEndedState();

  double current_time = media_source_->GetMediaElement()->current_time(NULL);
  if (!chunk_demuxer_->EvictCodedFrames(
          id_, base::TimeDelta::FromSecondsD(current_time),
          new_data_size + evict_extra_in_bytes_)) {
    web::DOMException::Raise(web::DOMException::kQuotaExceededErr,
                             exception_state);
    metrics_.EndTracking(SourceBufferMetricsAction::PREPARE_APPEND, 0);
    return false;
  }

  metrics_.EndTracking(SourceBufferMetricsAction::PREPARE_APPEND, 0);
  return true;
}

void SourceBuffer::AppendBufferInternal(
    const unsigned char* data, size_t size,
    script::ExceptionState* exception_state) {
  TRACE_EVENT1("cobalt::dom", "SourceBuffer::AppendBufferInternal()", "size",
               size);
  if (!PrepareAppend(size, exception_state)) {
    return;
  }

  DCHECK(data || size == 0);

  if (data) {
    if (is_avoid_copying_array_buffer_enabled_) {
      // When |is_avoid_copying_array_buffer_enabled_| is true, we are holding
      // reference to the underlying JS buffer object, and don't have to make a
      // copy of the data.
      if (array_buffer_view_in_use_.IsEmpty()) {
        DCHECK(!array_buffer_in_use_.IsEmpty());
        DCHECK_EQ(array_buffer_in_use_->Data(), data);
        DCHECK_EQ(array_buffer_in_use_->ByteLength(), size);
      } else {
        DCHECK_EQ(array_buffer_view_in_use_->RawData(), data);
        DCHECK_EQ(array_buffer_view_in_use_->ByteLength(), size);
      }
    } else {
      if (pending_append_data_capacity_ < size) {
        pending_append_data_.reset();
        pending_append_data_.reset(new uint8_t[size]);
        pending_append_data_capacity_ = size;
      }
      memcpy(pending_append_data_.get(), data, size);
      data = pending_append_data_.get();
    }
  }

  ScheduleEvent(base::Tokens::updatestart());

  std::unique_ptr<SourceBufferAlgorithm> algorithm(
      new SourceBufferAppendAlgorithm(
          media_source_, chunk_demuxer_, id_, data, size,
          max_append_buffer_size_, DoubleToTimeDelta(append_window_start_),
          DoubleToTimeDelta(append_window_end_),
          DoubleToTimeDelta(timestamp_offset_),
          base::Bind(&SourceBuffer::UpdateTimestampOffset,
                     base::Unretained(this)),
          base::Bind(&SourceBuffer::ScheduleEvent, base::Unretained(this)),
          base::Bind(&SourceBuffer::OnAlgorithmFinalized,
                     base::Unretained(this)),
          &metrics_));
  auto algorithm_runner = media_source_->GetAlgorithmRunner(size);
  active_algorithm_handle_ =
      algorithm_runner->CreateHandle(std::move(algorithm));
  algorithm_runner->Start(active_algorithm_handle_);
}

void SourceBuffer::OnAlgorithmFinalized() {
  DCHECK(active_algorithm_handle_);
  active_algorithm_handle_ = nullptr;

  if (is_avoid_copying_array_buffer_enabled_) {
    // Allow them to be GCed.
    array_buffer_in_use_ = script::Handle<ArrayBuffer>();
    array_buffer_view_in_use_ = script::Handle<ArrayBufferView>();
    is_avoid_copying_array_buffer_enabled_ = false;
  } else {
    DCHECK(array_buffer_in_use_.IsEmpty());
    DCHECK(array_buffer_view_in_use_.IsEmpty());
  }
}

void SourceBuffer::UpdateTimestampOffset(base::TimeDelta timestamp_offset) {
  starboard::ScopedLock scoped_lock(timestamp_offset_mutex_);
  // The check avoids overwriting |timestamp_offset_| when there is a small
  // difference between its float and its int64_t representation .
  if (DoubleToTimeDelta(timestamp_offset_) != timestamp_offset) {
    timestamp_offset_ = timestamp_offset.InSecondsF();
  }
}

}  // namespace dom
}  // namespace cobalt
