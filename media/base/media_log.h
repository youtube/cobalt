// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_H_
#define MEDIA_BASE_MEDIA_LOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "media/base/media_log_event.h"

namespace media {

class MediaLog : public base::RefCountedThreadSafe<MediaLog> {
 public:

  // Return a string to represent an EventType.
  static const char* EventTypeToString(MediaLogEvent::Type type);

  MediaLog();

  // Methods called by loggers when events occur. These generate appropriate
  // event parameters so the caller need not worry about them.
  void Load(const std::string& url);

  // Add an event to this log. Overriden by inheritors to actually do something
  // with it.
  // Takes ownership of |event|.
  virtual void AddEvent(MediaLogEvent* event);

  // Convenience method for adding an event with no parameters.
  void AddEventOfType(MediaLogEvent::Type type);

  // Convenience method for filling in common fields of a new event.
  MediaLogEvent* CreateEvent(MediaLogEvent::Type type);

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
