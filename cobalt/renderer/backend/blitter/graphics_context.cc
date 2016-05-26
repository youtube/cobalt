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
#include "cobalt/renderer/backend/blitter/texture.h"
#include "cobalt/renderer/backend/blitter/texture_data.h"
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

scoped_ptr<Texture> GraphicsContextBlitter::CreateTexture(
    scoped_ptr<TextureData> texture_data) {
  scoped_ptr<TextureDataBlitter> texture_data_blitter(
      base::polymorphic_downcast<TextureDataBlitter*>(texture_data.release()));

  return scoped_ptr<Texture>(
      new TextureBlitter(device_, texture_data_blitter.Pass()));
}

scoped_ptr<Texture> GraphicsContextBlitter::CreateTextureFromRawMemory(
    const scoped_refptr<ConstRawTextureMemory>& raw_texture_memory,
    intptr_t offset, const SurfaceInfo& surface_info, int pitch_in_bytes) {
  NOTREACHED();
  return scoped_ptr<Texture>();
}

scoped_refptr<RenderTarget> GraphicsContextBlitter::CreateOffscreenRenderTarget(
    const math::Size& dimensions) {
  return scoped_refptr<RenderTarget>(
      new SurfaceRenderTargetBlitter(device_, dimensions));
}

scoped_ptr<Texture>
GraphicsContextBlitter::CreateTextureFromOffscreenRenderTarget(
    const scoped_refptr<RenderTarget>& render_target) {
  return scoped_ptr<Texture>(
      new TextureBlitter(scoped_refptr<SurfaceRenderTargetBlitter>(
          base::polymorphic_downcast<SurfaceRenderTargetBlitter*>(
              render_target.get()))));
}

scoped_array<uint8_t> GraphicsContextBlitter::GetCopyOfTexturePixelDataAsRGBA(
    const Texture& texture) {
  const TextureBlitter* texture_blitter =
      base::polymorphic_downcast<const TextureBlitter*>(&texture);

  SbBlitterSurface surface = texture_blitter->GetSbBlitterSurface();

  scoped_array<uint8_t> pixels(
      new uint8_t[texture.GetSurfaceInfo().size.GetArea() * 4]);

  SbBlitterFlushContext(context_);
  SbBlitterDownloadSurfacePixelDataAsRGBA(surface, pixels.get());

  return pixels.Pass();
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
