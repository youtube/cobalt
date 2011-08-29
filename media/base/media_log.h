// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "media/base/media_log_event.h"
#include "media/base/pipeline_impl.h"
#include "media/base/pipeline_status.h"

namespace media {

class MEDIA_EXPORT MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:
  // Convert various enums to strings.
  static const char* EventTypeToString(MediaLogEvent::Type type);
  static const char* PipelineStateToString(PipelineImpl::State);
  static const char* PipelineStatusToString(PipelineStatus);

  MediaLog();

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  // Takes ownership of |event|.
  virtual void AddEvent(MediaLogEvent* event);

  // Helper methods to create events and their parameters.
  MediaLogEvent* CreateEvent(MediaLogEvent::Type type);
  MediaLogEvent* CreateBooleanEvent(MediaLogEvent::Type type,
                                    const char* property, bool value);
  MediaLogEvent* CreateIntegerEvent(MediaLogEvent::Type type,
                                    const char* property, int64 value);
  MediaLogEvent* CreateTimeEvent(MediaLogEvent::Type type,
                                 const char* property, base::TimeDelta value);
  MediaLogEvent* CreateLoadEvent(const std::string& url);
  MediaLogEvent* CreateSeekEvent(float seconds);
  MediaLogEvent* CreatePipelineStateChangedEvent(PipelineImpl::State state);
  MediaLogEvent* CreatePipelineErrorEvent(PipelineStatus error);
  MediaLogEvent* CreateVideoSizeSetEvent(size_t width, size_t height);
  MediaLogEvent* CreateBufferedExtentsChangedEvent(size_t start, size_t current,
                                                   size_t end);

  // Called when the pipeline statistics have been updated.
  // This gets called every frame, so we send the most recent stats after 500ms.
  // This function is NOT thread safe.
  void QueueStatisticsUpdatedEvent(PipelineStatistics stats);

 protected:
  friend class base::RefCountedThreadSafe<MediaLog>;
  virtual ~MediaLog();

 private:
  // Actually add a STATISTICS_UPDATED event.
  void AddStatisticsUpdatedEvent();

  // A unique (to this process) id for this MediaLog.
  int32 id_;

  // The most recent set of pipeline stats.
  PipelineStatistics last_statistics_;
  bool stats_update_pending_;
  base::Lock stats_lock_;

  DISALLOW_COPY_AND_ASSIGN(MediaLog);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_H_
