// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_EVENT_H_
#define MEDIA_BASE_MEDIA_LOG_EVENT_H_
#pragma once

#include "base/time.h"
#include "base/values.h"

namespace media {

struct MediaLogEvent {
  enum Type {
    // A WebMediaPlayer is being created or destroyed.
    // params: none.
    WEBMEDIAPLAYER_CREATED,
    WEBMEDIAPLAYER_DESTROYED,

    // A PipelineImpl is being created or destroyed.
    // params: none.
    PIPELINE_CREATED,
    PIPELINE_DESTROYED,

    // A media player is loading a resource.
    // params: "url": <URL of the resource>.
    LOAD,

    // A media player has started seeking.
    // params: "seek_target": <number of seconds to which to seek>.
    SEEK,

    // A media player has been told to play or pause.
    // params: none.
    PLAY,
    PAUSE,

    // The state of PipelineImpl has changed.
    // params: "pipeline_state": <string name of the state>.
    PIPELINE_STATE_CHANGED,

    // The extents of the sliding buffer have changed.
    // params: "buffer_start": <first buffered byte>.
    //         "buffer_current": <current offset>.
    //         "buffer_end": <last buffered byte>.
    BUFFERED_EXTENTS_CHANGED,
  };

  int32 id;
  Type type;
  base::DictionaryValue params;
  base::Time time;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_EVENT_H_
