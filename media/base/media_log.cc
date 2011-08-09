// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"

namespace media {

// A count of all MediaLogs created on this render process.
// Used to generate unique ids.
static base::AtomicSequenceNumber media_log_count(base::LINKER_INITIALIZED);

const char* MediaLog::EventTypeToString(MediaLogEvent::Type type) {
  switch (type) {
    case MediaLogEvent::WEBMEDIAPLAYER_CREATED:
      return "WEBMEDIAPLAYER_CREATED";
    case MediaLogEvent::WEBMEDIAPLAYER_DESTROYED:
      return "WEBMEDIAPLAYER_DESTROYED";
    case MediaLogEvent::PIPELINE_CREATED:
      return "PIPELINE_CREATED";
    case MediaLogEvent::PIPELINE_DESTROYED:
      return "PIPELINE_DESTROYED";
    case MediaLogEvent::LOAD:
      return "LOAD";
    case MediaLogEvent::SEEK:
      return "SEEK";
    case MediaLogEvent::PLAY:
      return "PLAY";
    case MediaLogEvent::PAUSE:
      return "PAUSE";
    case MediaLogEvent::PIPELINE_STATE_CHANGED:
      return "PIPELINE_STATE_CHANGED";
    case MediaLogEvent::BUFFERED_EXTENTS_CHANGED:
      return "BUFFERED_EXTENTS_CHANGED";
  }
  NOTREACHED();
  return NULL;
}

const char* MediaLog::PipelineStateToString(PipelineImpl::State state) {
  switch (state) {
    case PipelineImpl::kCreated:
      return "created";
    case PipelineImpl::kInitDemuxer:
      return "initDemuxer";
    case PipelineImpl::kInitAudioDecoder:
      return "initAudioDecoder";
    case PipelineImpl::kInitAudioRenderer:
      return "initAudioRenderer";
    case PipelineImpl::kInitVideoDecoder:
      return "initVideoDecoder";
    case PipelineImpl::kInitVideoRenderer:
      return "initVideoRenderer";
    case PipelineImpl::kPausing:
      return "pausing";
    case PipelineImpl::kSeeking:
      return "seeking";
    case PipelineImpl::kFlushing:
      return "flushing";
    case PipelineImpl::kStarting:
      return "starting";
    case PipelineImpl::kStarted:
      return "started";
    case PipelineImpl::kEnded:
      return "ended";
    case PipelineImpl::kStopping:
      return "stopping";
    case PipelineImpl::kStopped:
      return "stopped";
    case PipelineImpl::kError:
      return "error";
  }
  NOTREACHED();
  return NULL;
}

MediaLog::MediaLog() {
  id_ = media_log_count.GetNext();
}

MediaLog::~MediaLog() {}

void MediaLog::AddEvent(MediaLogEvent* event) {
  scoped_ptr<MediaLogEvent> e(event);
}

MediaLogEvent* MediaLog::CreateEvent(MediaLogEvent::Type type) {
  scoped_ptr<MediaLogEvent> event(new MediaLogEvent);
  event->id = id_;
  event->type = type;
  event->time = base::Time::Now();
  return event.release();
}

MediaLogEvent* MediaLog::CreateLoadEvent(const std::string& url) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::LOAD));
  event->params.SetString("url", url);
  return event.release();
}

MediaLogEvent* MediaLog::CreateSeekEvent(float seconds) {
  scoped_ptr<MediaLogEvent> event(CreateEvent(MediaLogEvent::SEEK));
  event->params.SetDouble("seek_target", seconds);
  return event.release();
}

MediaLogEvent* MediaLog::CreatePipelineStateChangedEvent(
    PipelineImpl::State state) {
  scoped_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::PIPELINE_STATE_CHANGED));
  event->params.SetString("pipeline_state", PipelineStateToString(state));
  return event.release();
}

MediaLogEvent* MediaLog::CreateBufferedExtentsChangedEvent(
    size_t start, size_t current, size_t end) {
  scoped_ptr<MediaLogEvent> event(
      CreateEvent(MediaLogEvent::BUFFERED_EXTENTS_CHANGED));
  event->params.SetInteger("buffer_start", start);
  event->params.SetInteger("buffer_current", current);
  event->params.SetInteger("buffer_end", end);
  return event.release();
}

}  //namespace media
