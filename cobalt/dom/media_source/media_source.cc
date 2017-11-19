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

#include "cobalt/dom/media_source/media_source.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event.h"
#include "cobalt/media/base/pipeline_status.h"
#include "starboard/double.h"
#include "starboard/media.h"

namespace cobalt {
namespace dom {

#if defined(COBALT_MEDIA_SOURCE_2016)
using media::PipelineStatus;
using media::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
using media::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
using media::PIPELINE_OK;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
using ::media::PipelineStatus;
using ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
using ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
using ::media::PIPELINE_OK;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace {

// Parse mime and codecs from content type. It will return "video/mp4" and
// "avc1.42E01E, mp4a.40.2" for "video/mp4; codecs="avc1.42E01E, mp4a.40.2".
// Note that this function does minimum validation as the media stack will check
// the mime type and codecs strictly.
bool ParseContentType(const std::string& content_type, std::string* mime,
                      std::string* codecs) {
  DCHECK(mime);
  DCHECK(codecs);
  static const char kCodecs[] = "codecs=";

  std::vector<std::string> tokens;
  // SplitString will also trim the results.
  ::base::SplitString(content_type, ';', &tokens);
  // The first one has to be mime type with delimiter '/' like 'video/mp4'.
  if (tokens.size() < 2 || tokens[0].find('/') == tokens[0].npos) {
    return false;
  }
  *mime = tokens[0];
  for (size_t i = 1; i < tokens.size(); ++i) {
    if (base::strncasecmp(tokens[i].c_str(), kCodecs, strlen(kCodecs))) {
      continue;
    }
    *codecs = tokens[i].substr(strlen("codecs="));
    TrimString(*codecs, " \"", codecs);
    break;
  }
  return !codecs->empty();
}

}  // namespace

MediaSource::MediaSource()
    : chunk_demuxer_(NULL),
      ready_state_(kMediaSourceReadyStateClosed),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      source_buffers_(new SourceBufferList(&event_queue_)),
      active_source_buffers_(new SourceBufferList(&event_queue_)),
      live_seekable_range_(new TimeRanges) {}

MediaSource::~MediaSource() { SetReadyState(kMediaSourceReadyStateClosed); }

scoped_refptr<SourceBufferList> MediaSource::source_buffers() const {
  return source_buffers_;
}

scoped_refptr<SourceBufferList> MediaSource::active_source_buffers() const {
  return active_source_buffers_;
}

MediaSourceReadyState MediaSource::ready_state() const { return ready_state_; }

double MediaSource::duration(script::ExceptionState* exception_state) const {
  UNREFERENCED_PARAMETER(exception_state);

  if (ready_state_ == kMediaSourceReadyStateClosed) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  DCHECK(chunk_demuxer_);
  return chunk_demuxer_->GetDuration();
}

void MediaSource::set_duration(double duration,
                               script::ExceptionState* exception_state) {
  if (duration < 0.0 || SbDoubleIsNan(duration)) {
    DOMException::Raise(DOMException::kIndexSizeErr, exception_state);
    return;
  }
  if (!IsOpen() || IsUpdating()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // Run the duration change algorithm
  if (duration == this->duration(NULL)) {
    return;
  }

  double highest_buffered_presentation_timestamp = 0;
  for (uint32 i = 0; i < source_buffers_->length(); ++i) {
    highest_buffered_presentation_timestamp =
        std::max(highest_buffered_presentation_timestamp,
                 source_buffers_->Item(i)->GetHighestPresentationTimestamp());
  }

  if (duration < highest_buffered_presentation_timestamp) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // 3. Set old duration to the current value of duration.
  double old_duration = this->duration(NULL);
  DCHECK_LE(highest_buffered_presentation_timestamp,
            std::isnan(old_duration) ? 0 : old_duration);

  // 4. Update duration to new duration.
  bool request_seek = attached_element_->current_time(NULL) > duration;
  chunk_demuxer_->SetDuration(duration);

  // 5. If a user agent is unable to partially render audio frames or text cues
  //    that start before and end after the duration, then run the following
  //    steps:
  //    NOTE: Currently we assume that the media engine is able to render
  //    partial frames/cues. If a media engine gets added that doesn't support
  //    this, then we'll need to add logic to handle the substeps.

  // 6. Update the media controller duration to new duration and run the
  //    HTMLMediaElement duration change algorithm.
  attached_element_->DurationChanged(duration, request_seek);
}

scoped_refptr<SourceBuffer> MediaSource::AddSourceBuffer(
    const std::string& type, script::ExceptionState* exception_state) {
  DLOG(INFO) << "add SourceBuffer with type " << type;

  if (type.empty()) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  if (!IsTypeSupported(NULL, type)) {
    DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
    return NULL;
  }

  if (!IsOpen()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return NULL;
  }

  std::string mime;
  std::string codecs;

  if (!ParseContentType(type, &mime, &codecs)) {
    DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  std::string guid = base::GenerateGUID();
  scoped_refptr<SourceBuffer> source_buffer;
  ChunkDemuxer::Status status = chunk_demuxer_->AddId(guid, mime, codecs);
  switch (status) {
    case ChunkDemuxer::kOk:
      source_buffer =
          new SourceBuffer(guid, this, chunk_demuxer_, &event_queue_);
      break;
    case ChunkDemuxer::kNotSupported:
      DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
      return NULL;
    case ChunkDemuxer::kReachedIdLimit:
      DOMException::Raise(DOMException::kQuotaExceededErr, exception_state);
      return NULL;
  }

  DCHECK(source_buffer);
  source_buffers_->Add(source_buffer);
  return source_buffer;
}

void MediaSource::RemoveSourceBuffer(
    const scoped_refptr<SourceBuffer>& source_buffer,
    script::ExceptionState* exception_state) {
  if (source_buffer == NULL) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }

  if (source_buffers_->length() == 0 ||
      !source_buffers_->Contains(source_buffer)) {
    DOMException::Raise(DOMException::kNotFoundErr, exception_state);
    return;
  }

  source_buffer->OnRemovedFromMediaSource();

  active_source_buffers_->Remove(source_buffer);
  source_buffers_->Remove(source_buffer);
}

void MediaSource::EndOfStream(script::ExceptionState* exception_state) {
  // If there is no error string provided, treat it as empty.
  EndOfStream(kMediaSourceEndOfStreamErrorNoError, exception_state);
}

void MediaSource::EndOfStream(MediaSourceEndOfStreamError error,
                              script::ExceptionState* exception_state) {
  if (!IsOpen() || IsUpdating()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  SetReadyState(kMediaSourceReadyStateEnded);

  PipelineStatus pipeline_status = PIPELINE_OK;

  if (error == kMediaSourceEndOfStreamErrorNetwork) {
    pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
  } else if (error == kMediaSourceEndOfStreamErrorDecode) {
    pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
  }

  chunk_demuxer_->MarkEndOfStream(pipeline_status);
}

void MediaSource::SetLiveSeekableRange(
    double start, double end, script::ExceptionState* exception_state) {
  if (!IsOpen()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (start < 0 || start > end) {
    DOMException::Raise(DOMException::kIndexSizeErr, exception_state);
    return;
  }

  live_seekable_range_ = new TimeRanges(start, end);
}

void MediaSource::ClearLiveSeekableRange(
    script::ExceptionState* exception_state) {
  if (!IsOpen()) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (live_seekable_range_->length() != 0) {
    live_seekable_range_ = new TimeRanges;
  }
}

// static
bool MediaSource::IsTypeSupported(script::EnvironmentSettings* settings,
                                  const std::string& type) {
  // TODO: Remove |settings| parameter once MSE2012 is removed.
  UNREFERENCED_PARAMETER(settings);
  SbMediaSupportType support_type =
      SbMediaCanPlayMimeAndKeySystem(type.c_str(), "");
  if (support_type == kSbMediaSupportTypeNotSupported) {
    LOG(INFO) << "MediaSource::IsTypeSupported(" << type
              << ") -> not supported/false";
    return false;
  }
  if (support_type == kSbMediaSupportTypeMaybe) {
    LOG(INFO) << "MediaSource::IsTypeSupported(" << type << ") -> maybe/true";
    return true;
  }
  if (support_type == kSbMediaSupportTypeProbably) {
    LOG(INFO) << "MediaSource::IsTypeSupported(" << type
              << ") -> probably/true";
    return true;
  }
  NOTREACHED();
  return false;
}

bool MediaSource::AttachToElement(HTMLMediaElement* media_element) {
  if (attached_element_) {
    return false;
  }

  DCHECK(IsClosed());
  attached_element_ = base::AsWeakPtr(media_element);
  return true;
}

void MediaSource::SetChunkDemuxerAndOpen(ChunkDemuxer* chunk_demuxer) {
  DCHECK(chunk_demuxer);
  DCHECK(!chunk_demuxer_);
  DCHECK(attached_element_);
  chunk_demuxer_ = chunk_demuxer;
  SetReadyState(kMediaSourceReadyStateOpen);
}

void MediaSource::Close() { SetReadyState(kMediaSourceReadyStateClosed); }

bool MediaSource::IsClosed() const {
  return ready_state_ == kMediaSourceReadyStateClosed;
}

scoped_refptr<TimeRanges> MediaSource::GetBufferedRange() const {
  std::vector<scoped_refptr<TimeRanges> > ranges(
      active_source_buffers_->length());
  for (uint32 i = 0; i < active_source_buffers_->length(); ++i)
    ranges[i] = active_source_buffers_->Item(i)->buffered(NULL);

  if (ranges.empty()) {
    return new TimeRanges;
  }

  double highest_end_time = -1;
  for (size_t i = 0; i < ranges.size(); ++i) {
    uint32 length = ranges[i]->length();
    if (length > 0) {
      highest_end_time =
          std::max(highest_end_time, ranges[i]->End(length - 1, NULL));
    }
  }

  // Return an empty range if all ranges are empty.
  if (highest_end_time < 0) {
    return new TimeRanges;
  }

  scoped_refptr<TimeRanges> intersection_ranges =
      new TimeRanges(0, highest_end_time);

  bool ended = ready_state() == kMediaSourceReadyStateEnded;
  for (size_t i = 0; i < ranges.size(); ++i) {
    scoped_refptr<TimeRanges> source_ranges = ranges[i].get();

    if (ended && source_ranges->length()) {
      source_ranges->Add(
          source_ranges->Start(source_ranges->length() - 1, NULL),
          highest_end_time);
    }

    intersection_ranges = intersection_ranges->IntersectWith(source_ranges);
  }

  return intersection_ranges;
}

scoped_refptr<TimeRanges> MediaSource::GetSeekable() const {
  // Implements MediaSource algorithm for HTMLMediaElement.seekable.
  double source_duration = duration(NULL);

  if (std::isnan(source_duration)) {
    return new TimeRanges;
  }

  if (source_duration == std::numeric_limits<double>::infinity()) {
    scoped_refptr<TimeRanges> buffered = attached_element_->buffered();

    if (live_seekable_range_->length() != 0) {
      if (buffered->length() == 0) {
        return new TimeRanges(live_seekable_range_->Start(0, NULL),
                              live_seekable_range_->End(0, NULL));
      }

      return new TimeRanges(
          std::min(live_seekable_range_->Start(0, NULL),
                   buffered->Start(0, NULL)),
          std::max(live_seekable_range_->End(0, NULL),
                   buffered->End(buffered->length() - 1, NULL)));
    }

    if (buffered->length() == 0) {
      return new TimeRanges;
    }

    return new TimeRanges(0, buffered->End(buffered->length() - 1, NULL));
  }

  return new TimeRanges(0, source_duration);
}

void MediaSource::OnAudioTrackChanged(AudioTrack* audio_track) {
  scoped_refptr<SourceBuffer> source_buffer = audio_track->source_buffer();

  if (!source_buffer) {
    return;
  }

  DCHECK(source_buffers_->Contains(source_buffer));
  source_buffer->audio_tracks()->ScheduleChangeEvent();

  bool is_active = (source_buffer->video_tracks()->selected_index() != -1) ||
                   source_buffer->audio_tracks()->HasEnabledTrack();
  SetSourceBufferActive(source_buffer, is_active);
}

void MediaSource::OnVideoTrackChanged(VideoTrack* video_track) {
  scoped_refptr<SourceBuffer> source_buffer = video_track->source_buffer();

  if (!source_buffer) {
    return;
  }

  DCHECK(source_buffers_->Contains(source_buffer));
  if (video_track->selected()) {
    source_buffer->video_tracks()->OnTrackSelected(video_track->id());
  }
  source_buffer->video_tracks()->ScheduleChangeEvent();

  bool is_active = source_buffer->video_tracks()->selected_index() != -1 ||
                   source_buffer->audio_tracks()->HasEnabledTrack();

  SetSourceBufferActive(source_buffer, is_active);
}

void MediaSource::OpenIfInEndedState() {
  if (ready_state_ != kMediaSourceReadyStateEnded) {
    return;
  }

  SetReadyState(kMediaSourceReadyStateOpen);
  chunk_demuxer_->UnmarkEndOfStream();
}

bool MediaSource::IsOpen() const {
  return ready_state_ == kMediaSourceReadyStateOpen;
}

void MediaSource::SetSourceBufferActive(SourceBuffer* source_buffer,
                                        bool is_active) {
  // We don't support deactivate a source buffer.
  DCHECK(is_active);
  DCHECK(source_buffers_->Contains(source_buffer));

  if (!is_active) {
    DCHECK(active_source_buffers_->Contains(source_buffer));
    active_source_buffers_->Remove(source_buffer);
    return;
  }

  if (active_source_buffers_->Contains(source_buffer)) {
    return;
  }

  size_t index = source_buffers_->Find(source_buffer);

  uint32 insert_position = 0;
  while (insert_position < active_source_buffers_->length() &&
         source_buffers_->Find(active_source_buffers_->Item(insert_position)) <
             index) {
    ++insert_position;
  }

  active_source_buffers_->Insert(insert_position, source_buffer);
}

HTMLMediaElement* MediaSource::GetMediaElement() const {
  return attached_element_;
}

void MediaSource::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  tracer->Trace(event_queue_);
  tracer->Trace(attached_element_);
  tracer->Trace(source_buffers_);
  tracer->Trace(active_source_buffers_);
  tracer->Trace(live_seekable_range_);
}

void MediaSource::SetReadyState(MediaSourceReadyState ready_state) {
  if (ready_state == kMediaSourceReadyStateClosed) {
    chunk_demuxer_ = NULL;
  }

  if (ready_state_ == ready_state) {
    return;
  }

  MediaSourceReadyState old_state = ready_state_;
  ready_state_ = ready_state;

  if (IsOpen()) {
    ScheduleEvent(base::Tokens::sourceopen());
    return;
  }

  if (old_state == kMediaSourceReadyStateOpen &&
      ready_state_ == kMediaSourceReadyStateEnded) {
    ScheduleEvent(base::Tokens::sourceended());
    return;
  }

  DCHECK(IsClosed());

  active_source_buffers_->Clear();

  // Clear SourceBuffer references to this object.
  for (uint32 i = 0; i < source_buffers_->length(); ++i) {
    source_buffers_->Item(i)->OnRemovedFromMediaSource();
  }
  source_buffers_->Clear();

  attached_element_.reset();

  ScheduleEvent(base::Tokens::sourceclose());
}

bool MediaSource::IsUpdating() const {
  // Return true if any member of |source_buffers_| is updating.
  for (uint32 i = 0; i < source_buffers_->length(); ++i) {
    if (source_buffers_->Item(i)->updating()) {
      return true;
    }
  }

  return false;
}

void MediaSource::ScheduleEvent(base::Token event_name) {
  scoped_refptr<Event> event = new Event(event_name);
  event->set_target(this);
  event_queue_.Enqueue(event);
}

}  // namespace dom
}  // namespace cobalt
