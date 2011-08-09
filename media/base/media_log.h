// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "media/base/media_log_event.h"
#include "media/base/pipeline_impl.h"

namespace media {

class MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:
  // Convert various enums to strings.
  static const char* EventTypeToString(MediaLogEvent::Type type);
  static const char* PipelineStateToString(PipelineImpl::State);

  MediaLog();

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  // Takes ownership of |event|.
  virtual void AddEvent(MediaLogEvent* event);

  // Helper methods to create events and their parameters.
  MediaLogEvent* CreateEvent(MediaLogEvent::Type type);
  MediaLogEvent* CreateLoadEvent(const std::string& url);
  MediaLogEvent* CreateSeekEvent(float seconds);
  MediaLogEvent* CreatePipelineStateChangedEvent(PipelineImpl::State state);
  MediaLogEvent* CreateBufferedExtentsChangedEvent(size_t start, size_t current,
                                                   size_t end);

 protected:
  friend class base::RefCountedThreadSafe<MediaLog>;
  virtual ~MediaLog();

 private:
  // A unique (to this process) id for this MediaLog.
  int32 id_;

  DISALLOW_COPY_AND_ASSIGN(MediaLog);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_H_
