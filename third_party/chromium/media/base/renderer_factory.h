// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_RENDERER_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_RENDERER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/media_resource.h"
#include "third_party/chromium/media/base/overlay_info.h"
#include "third_party/chromium/media/base/renderer.h"
#include "ui/gfx/color_space.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace media_m96 {

class AudioRendererSink;
class VideoRendererSink;

// A factory class for creating media_m96::Renderer to be used by media pipeline.
class MEDIA_EXPORT RendererFactory {
 public:
  RendererFactory();

  RendererFactory(const RendererFactory&) = delete;
  RendererFactory& operator=(const RendererFactory&) = delete;

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
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) = 0;

  // Returns the MediaResource::Type that should be used with the renderers
  // created by this factory.
  // NOTE: Returns Type::STREAM by default.
  virtual MediaResource::Type GetRequiredMediaResourceType();
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_RENDERER_FACTORY_H_
