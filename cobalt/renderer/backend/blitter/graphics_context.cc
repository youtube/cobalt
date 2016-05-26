/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/backend/blitter/graphics_context.h"

#include <algorithm>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/graphics_system.h"
#include "cobalt/renderer/backend/blitter/surface_render_target.h"
#include "starboard/blitter.h"

namespace cobalt {
namespace renderer {
namespace backend {

GraphicsContextBlitter::GraphicsContextBlitter(GraphicsSystem* system)
    : GraphicsContext(system) {
  device_ = base::polymorphic_downcast<GraphicsSystemBlitter*>(system)
                ->GetSbBlitterDevice();
  context_ = SbBlitterCreateContext(device_);
  CHECK(SbBlitterIsContextValid(context_));
}

GraphicsContextBlitter::~GraphicsContextBlitter() {
  SbBlitterDestroyContext(context_);
}

scoped_refptr<RenderTarget> GraphicsContextBlitter::CreateOffscreenRenderTarget(
    const math::Size& dimensions) {
  return scoped_refptr<RenderTarget>(
      new SurfaceRenderTargetBlitter(device_, dimensions));
}

scoped_array<uint8_t> GraphicsContextBlitter::DownloadPixelDataAsRGBA(
    const scoped_refptr<RenderTarget>& render_target) {
  SurfaceRenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<SurfaceRenderTargetBlitter*>(
          render_target.get());

  SbBlitterSurface surface = render_target_blitter->GetSbSurface();

  scoped_array<uint8_t> pixels(
      new uint8_t[render_target_blitter->GetSize().GetArea() * 4]);

  SbBlitterFlushContext(context_);
  SbBlitterDownloadSurfacePixelDataAsRGBA(surface, pixels.get());

  return pixels.Pass();
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
