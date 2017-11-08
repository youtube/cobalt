// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/backend/blitter/graphics_context.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/graphics_system.h"
#include "cobalt/renderer/backend/blitter/surface_render_target.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

GraphicsContextBlitter::GraphicsContextBlitter(GraphicsSystem* system)
    : GraphicsContext(system) {
  TRACE_EVENT0("cobalt::renderer",
               "GraphicsContextBlitter::GraphicsContextBlitter");

  device_ = base::polymorphic_downcast<GraphicsSystemBlitter*>(system)
                ->GetSbBlitterDevice();
  context_ = SbBlitterCreateContext(device_);
  CHECK(SbBlitterIsContextValid(context_));
}

GraphicsContextBlitter::~GraphicsContextBlitter() {
  TRACE_EVENT0("cobalt::renderer",
               "GraphicsContextBlitter::~GraphicsContextBlitter");
  SbBlitterDestroyContext(context_);
}

scoped_refptr<RenderTarget> GraphicsContextBlitter::CreateOffscreenRenderTarget(
    const math::Size& dimensions) {
  TRACE_EVENT0("cobalt::renderer",
               "GraphicsContextBlitter::CreateOffscreenRenderTarget");

  scoped_refptr<RenderTarget> render_target(
      new SurfaceRenderTargetBlitter(device_, dimensions));
  if (render_target->CreationError()) {
    return scoped_refptr<RenderTarget>();
  } else {
    return render_target;
  }
}

scoped_array<uint8_t> GraphicsContextBlitter::DownloadPixelDataAsRGBA(
    const scoped_refptr<RenderTarget>& render_target) {
  TRACE_EVENT0("cobalt::renderer",
               "GraphicsContextBlitter::DownloadPixelDataAsRGBA");
  SurfaceRenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<SurfaceRenderTargetBlitter*>(
          render_target.get());

  SbBlitterSurface surface = render_target_blitter->GetSbSurface();

  const math::Size& size = render_target_blitter->GetSize();
  scoped_array<uint8_t> pixels(new uint8_t[size.GetArea() * 4]);

  SbBlitterFlushContext(context_);
  SbBlitterDownloadSurfacePixels(surface, kSbBlitterPixelDataFormatRGBA8,
                                 size.width() * 4, pixels.get());

  return pixels.Pass();
}

void GraphicsContextBlitter::Finish() {
  // Note: flushing the context doesn't actually guarantee that drawing has
  // finished.
  SbBlitterFlushContext(context_);
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
