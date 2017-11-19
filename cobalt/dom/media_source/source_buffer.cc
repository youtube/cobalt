/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/media_source/source_buffer.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/time.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/base/timestamp_constants.h"

namespace cobalt {
namespace dom {

namespace {

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!std::isnan(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  if (time == std::numeric_limits<double>::infinity()) {
    return media::kInfiniteDuration;
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

}  // namespace

SourceBuffer::SourceBuffer(const std::string& id, MediaSource* media_source,
                           media::ChunkDemuxer* chunk_demuxer,
                           EventQueue* event_queue)
    : id_(id),
      chunk_demuxer_(chunk_demuxer),
      media_source_(media_source),
      track_defaults_(new TrackDefaultList(NULL)),
      event_queue_(event_queue),
      mode_(kSourceBufferAppendModeSegments),
      updating_(false),
      timestamp_offset_(0),
      audio_tracks_(new AudioTrackList(media_source->GetMediaElement())),
      video_tracks_(new VideoTrackList(media_source->GetMediaElement())),
      append_window_start_(0),
      append_window_end_(std::numeric_limits<double>::infinity()),
      first_initialization_segment_received_(false),
      pending_append_data_offset_(0),
      pending_remove_start_(-1),
      pending_remove_end_(-1) {
  DCHECK(!id_.empty());
  DCHECK(media_source_);
  DCHECK(chunk_demuxer);
  DCHECK(event_queue);

  chunk_demuxer_->SetTracksWatcher(
      id_, base::Bind(&SourceBuffer::InitSegmentReceived, this));
}

void SourceBuffer::set_mode(SourceBufferAppendMode mode,
                            script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  media_source_->OpenIfInEndedState();

  if (chunk_demuxer_->IsParsingMediaSegment(id_)) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  chunk_demuxer_->SetSequenceMode(id_, mode == kSourceBufferAppendModeSequence);

  mode_ = mode;
}

scoped_refptr<TimeRanges> SourceBuffer::buffered(
    script::ExceptionState* exception_state) const {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return NULL;
  }

  scoped_refptr<TimeRanges> time_ranges = new TimeRanges;
  media::Ranges<base::TimeDelta> ranges =
      chunk_demuxer_->GetBufferedRanges(id_);
  for (size_t i = 0; i < ranges.size(); i++) {
    time_ranges->Add(ranges.start(i).InSecondsF(), ranges.end(i).InSecondsF());
  }
  return time_ranges;
}

void SourceBuffer::set_timestamp_offset(
    double offset, script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  media_source_->OpenIfInEndedState();

  if (chunk_demuxer_->IsParsingMediaSegment(id_)) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  timestamp_offset_ = offset;

  chunk_demuxer_->SetGroupStartTimestampIfInSequenceMode(
      id_, DoubleToTimeDelta(timestamp_offset_));
}

void SourceBuffer::set_append_window_start(
    double start, script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
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
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
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

void SourceBuffer::AppendBuffer(const scoped_refptr<ArrayBuffer>& data,
                                script::ExceptionState* exception_state) {
  AppendBufferInternal(static_cast<const unsigned char*>(data->data()),
                       data->byte_length(), exception_state);
}

void SourceBuffer::AppendBuffer(const scoped_refptr<ArrayBufferView>& data,
                                script::ExceptionState* exception_state) {
  AppendBufferInternal(static_cast<const unsigned char*>(data->base_address()),
                       data->byte_length(), exception_state);
}

void SourceBuffer::Abort(script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (!media_source_->IsOpen()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (pending_remove_start_ != -1) {
    DCHECK(updating_);
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  AbortIfUpdating();

  base::TimeDelta timestamp_offset = DoubleToTimeDelta(timestamp_offset_);
  chunk_demuxer_->ResetParserState(id_, DoubleToTimeDelta(append_window_start_),
                                   DoubleToTimeDelta(append_window_end_),
                                   &timestamp_offset);
  timestamp_offset_ = timestamp_offset.InSecondsF();

  set_append_window_start(0, exception_state);
  set_append_window_end(std::numeric_limits<double>::infinity(),
                        exception_state);
}

void SourceBuffer::Remove(double start, double end,
                          script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
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

  updating_ = true;

  ScheduleEvent(base::Tokens::updatestart());

  pending_remove_start_ = start;
  pending_remove_end_ = end;
  remove_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                      &SourceBuffer::OnRemoveTimer);
}

void SourceBuffer::set_track_defaults(
    const scoped_refptr<TrackDefaultList>& track_defaults,
    script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  track_defaults_ = track_defaults;
}

void SourceBuffer::OnRemovedFromMediaSource() {
  if (media_source_ == NULL) {
    return;
  }

  if (pending_remove_start_ != -1) {
    CancelRemove();
  } else {
    AbortIfUpdating();
  }

  DCHECK(media_source_);

  // TODO: Implement track support.
  // if (media_source_->GetMediaElement()->audio_tracks().length() > 0 ||
  //     media_source_->GetMediaElement()->audio_tracks().length() > 0) {
  //   RemoveMediaTracks();
  // }

  chunk_demuxer_->RemoveId(id_);
  chunk_demuxer_ = NULL;
  media_source_ = NULL;
  event_queue_ = NULL;
}

double SourceBuffer::GetHighestPresentationTimestamp() const {
  DCHECK(media_source_ != NULL);

  return chunk_demuxer_->GetHighestPresentationTimestamp(id_).InSecondsF();
}

void SourceBuffer::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(media_source_);
  tracer->Trace(track_defaults_);
  tracer->Trace(audio_tracks_);
  tracer->Trace(video_tracks_);
}

void SourceBuffer::InitSegmentReceived(scoped_ptr<MediaTracks> tracks) {
  UNREFERENCED_PARAMETER(tracks);

  if (!first_initialization_segment_received_) {
    media_source_->SetSourceBufferActive(this, true);
    first_initialization_segment_received_ = true;
  }

  // TODO: Implement track support.
}

void SourceBuffer::ScheduleEvent(base::Token event_name) {
  scoped_refptr<Event> event = new Event(event_name);
  event->set_target(this);
  event_queue_->Enqueue(event);
}

bool SourceBuffer::PrepareAppend(size_t new_data_size,
                                 script::ExceptionState* exception_state) {
  if (media_source_ == NULL) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return false;
  }
  if (updating_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return false;
  }

  DCHECK(media_source_->GetMediaElement());
  if (media_source_->GetMediaElement()->error()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return false;
  }

  media_source_->OpenIfInEndedState();

  if (!EvictCodedFrames(new_data_size)) {
    DOMException::Raise(DOMException::kQuotaExceededErr, exception_state);
    return false;
  }

  return true;
}

bool SourceBuffer::EvictCodedFrames(size_t new_data_size) {
  DCHECK(media_source_);
  DCHECK(media_source_->GetMediaElement());
  double current_time = media_source_->GetMediaElement()->current_time(NULL);
  return chunk_demuxer_->EvictCodedFrames(
      id_, base::TimeDelta::FromSecondsD(current_time), new_data_size);
}

void SourceBuffer::AppendBufferInternal(
    const unsigned char* data, uint32 size,
    script::ExceptionState* exception_state) {
  if (!PrepareAppend(size, exception_state)) {
    return;
  }

  DCHECK(data || size == 0);
  if (data) {
    pending_append_data_.insert(pending_append_data_.end(), data, data + size);
  }
  pending_append_data_offset_ = 0;

  updating_ = true;

  ScheduleEvent(base::Tokens::updatestart());

  append_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                      &SourceBuffer::OnAppendTimer);
}

void SourceBuffer::OnAppendTimer() {
  const size_t kMaxAppendSize = 128 * 1024;

  DCHECK(updating_);

  DCHECK_GE(pending_append_data_.size(), pending_append_data_offset_);
  size_t append_size =
      pending_append_data_.size() - pending_append_data_offset_;
  append_size = std::min(append_size, kMaxAppendSize);

  uint8 dummy[1];
  const uint8* data_to_append =
      append_size > 0 ? &pending_append_data_[0] + pending_append_data_offset_
                      : dummy;

  base::TimeDelta timestamp_offset = DoubleToTimeDelta(timestamp_offset_);
  bool success = chunk_demuxer_->AppendData(
      id_, data_to_append, append_size, DoubleToTimeDelta(append_window_start_),
      DoubleToTimeDelta(append_window_end_), &timestamp_offset);

  if (timestamp_offset != DoubleToTimeDelta(timestamp_offset_)) {
    timestamp_offset_ = timestamp_offset.InSecondsF();
  }

  if (!success) {
    pending_append_data_.clear();
    pending_append_data_offset_ = 0;
    AppendError();
  } else {
    pending_append_data_offset_ += append_size;

    if (pending_append_data_offset_ < pending_append_data_.size()) {
      append_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                          &SourceBuffer::OnAppendTimer);
      return;
    }

    updating_ = false;
    pending_append_data_.clear();
    pending_append_data_offset_ = 0;

    ScheduleEvent(base::Tokens::update());
    ScheduleEvent(base::Tokens::updateend());
  }
}

void SourceBuffer::AppendError() {
  base::TimeDelta timestamp_offset = DoubleToTimeDelta(timestamp_offset_);
  chunk_demuxer_->ResetParserState(id_, DoubleToTimeDelta(append_window_start_),
                                   DoubleToTimeDelta(append_window_end_),
                                   &timestamp_offset);
  timestamp_offset_ = timestamp_offset.InSecondsF();

  updating_ = false;

  ScheduleEvent(base::Tokens::error());
  ScheduleEvent(base::Tokens::updateend());
  media_source_->EndOfStream(kMediaSourceEndOfStreamErrorDecode, NULL);
}

void SourceBuffer::OnRemoveTimer() {
  DCHECK(updating_);
  DCHECK_GE(pending_remove_start_, 0);
  DCHECK_LT(pending_remove_start_, pending_remove_end_);

  chunk_demuxer_->Remove(id_, DoubleToTimeDelta(pending_remove_start_),
                         DoubleToTimeDelta(pending_remove_end_));

  updating_ = false;
  pending_remove_start_ = -1;
  pending_remove_end_ = -1;

  ScheduleEvent(base::Tokens::update());
  ScheduleEvent(base::Tokens::updateend());
}

void SourceBuffer::CancelRemove() {
  DCHECK(updating_);
  DCHECK_NE(pending_remove_start_, -1);
  remove_timer_.Stop();
  pending_remove_start_ = -1;
  pending_remove_end_ = -1;
  updating_ = false;
}

void SourceBuffer::AbortIfUpdating() {
  if (!updating_) {
    return;
  }

  DCHECK_EQ(pending_remove_start_, -1);

  append_timer_.Stop();
  pending_append_data_.clear();
  pending_append_data_offset_ = 0;

  updating_ = false;

  ScheduleEvent(base::Tokens::abort());
  ScheduleEvent(base::Tokens::updateend());
}

void SourceBuffer::RemoveMediaTracks() { NOTREACHED(); }

}  // namespace dom
}  // namespace cobalt
