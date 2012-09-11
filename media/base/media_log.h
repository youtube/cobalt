// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_log_event.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"

namespace media {

class MEDIA_EXPORT MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:
  // Convert various enums to strings.
  static const char* EventTypeToString(MediaLogEvent::Type type);
  static const char* PipelineStatusToString(PipelineStatus);

  MediaLog();

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  virtual void AddEvent(scoped_ptr<MediaLogEvent> event);

  // Helper methods to create events and their parameters.
  scoped_ptr<MediaLogEvent> CreateEvent(MediaLogEvent::Type type);
  scoped_ptr<MediaLogEvent> CreateBooleanEvent(
      MediaLogEvent::Type type, const char* property, bool value);
  scoped_ptr<MediaLogEvent> CreateStringEvent(
      MediaLogEvent::Type type, const char* property, const std::string& value);
  scoped_ptr<MediaLogEvent> CreateTimeEvent(
      MediaLogEvent::Type type, const char* property, base::TimeDelta value);
  scoped_ptr<MediaLogEvent> CreateLoadEvent(const std::string& url);
  scoped_ptr<MediaLogEvent> CreateSeekEvent(float seconds);
  scoped_ptr<MediaLogEvent> CreatePipelineStateChangedEvent(
      Pipeline::State state);
  scoped_ptr<MediaLogEvent> CreatePipelineErrorEvent(PipelineStatus error);
  scoped_ptr<MediaLogEvent> CreateVideoSizeSetEvent(
      size_t width, size_t height);
  scoped_ptr<MediaLogEvent> CreateBufferedExtentsChangedEvent(
      size_t start, size_t current, size_t end);

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
