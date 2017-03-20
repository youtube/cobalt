// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_RENDERER_FACTORY_H_
#define COBALT_MEDIA_BASE_RENDERER_FACTORY_H_

#include <memory>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/renderer.h"
#include "cobalt/media/base/surface_manager.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace media {

class AudioRendererSink;
class VideoRendererSink;

// A factory class for creating media::Renderer to be used by media pipeline.
class MEDIA_EXPORT RendererFactory {
 public:
  RendererFactory();
  virtual ~RendererFactory();

  // Creates and returns a Renderer. All methods of the created Renderer except
  // for GetMediaTime() will be called on the |media_task_runner|.
  // GetMediaTime() could be called on any thread.
  // The created Renderer can use |audio_renderer_sink| to render audio and
  // |video_renderer_sink| to render video.
  virtual std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      const RequestSurfaceCB& request_surface_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererFactory);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_RENDERER_FACTORY_H_
