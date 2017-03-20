// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_RENDERER_CLIENT_H_
#define COBALT_MEDIA_BASE_RENDERER_CLIENT_H_

#include "base/time.h"
#include "cobalt/media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

// Interface used by Renderer, AudioRenderer, VideoRenderer and
// MediaPlayerRenderer implementations to notify their clients.
class RendererClient {
 public:
  // Executed if any error was encountered after Renderer initialization.
  virtual void OnError(PipelineStatus status) = 0;

  // Executed when rendering has reached the end of stream.
  virtual void OnEnded() = 0;

  // Executed periodically with rendering statistics.
  virtual void OnStatisticsUpdate(const PipelineStatistics& stats) = 0;

  // Executed when buffering state is changed.
  virtual void OnBufferingStateChange(BufferingState state) = 0;

  // Executed whenever the key needed to decrypt the stream is not available.
  virtual void OnWaitingForDecryptionKey() = 0;

  // Executed for the first video frame and whenever natural size changes.
  // Only used if media stream contains video track.
  virtual void OnVideoNaturalSizeChange(const gfx::Size& size) = 0;

  // Executed for the first video frame and whenever opacity changes.
  // Only used if media stream contains video track.
  virtual void OnVideoOpacityChange(bool opaque) = 0;

  // Executed when video metadata is first read, and whenever it changes.
  // Only used when we are using a URL demuxer (e.g. for MediaPlayerRenderer).
  virtual void OnDurationChange(base::TimeDelta duration) = 0;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_RENDERER_CLIENT_H_
