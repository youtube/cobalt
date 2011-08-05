// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_log.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"

namespace media {

// A count of all MediaLogs created on this render process.
// Used to generate unique ids.
static base::AtomicSequenceNumber media_log_count(base::LINKER_INITIALIZED);

const char* MediaLog::EventTypeToString(MediaLogEvent::Type type) {
  switch (type) {
    case MediaLogEvent::CREATING:
      return "CREATING";
    case MediaLogEvent::DESTROYING:
      return "DESTROYING";
    case MediaLogEvent::LOAD:
      return "LOAD";
    case MediaLogEvent::PLAY:
      return "PLAY";
    case MediaLogEvent::PAUSE:
      return "PAUSE";
  }
  NOTREACHED();
  return NULL;
}

MediaLog::MediaLog() {
  id_ = media_log_count.GetNext();
}

MediaLog::~MediaLog() {}

void MediaLog::Load(const std::string& url) {
  MediaLogEvent* event = CreateEvent(MediaLogEvent::LOAD);
  event->params.SetString("url", url);
  AddEvent(event);
}

void MediaLog::AddEventOfType(MediaLogEvent::Type type) {
  MediaLogEvent* event = CreateEvent(type);
  AddEvent(event);
}

MediaLogEvent* MediaLog::CreateEvent(MediaLogEvent::Type type) {
  MediaLogEvent* event = new MediaLogEvent;
  event->id = id_;
  event->type = type;
  event->time = base::Time::Now();
  return event;
}

void MediaLog::AddEvent(MediaLogEvent* event) {}

}  //namespace media
