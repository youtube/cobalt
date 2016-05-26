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

#ifndef COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_
#define COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/texture.h"
#include "starboard/blitter.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextBlitter : public GraphicsContext {
 public:
  explicit GraphicsContextBlitter(GraphicsSystem* system);
  ~GraphicsContextBlitter() OVERRIDE;

  scoped_ptr<Texture> CreateTexture(
      scoped_ptr<TextureData> texture_data) OVERRIDE;

  scoped_ptr<Texture> CreateTextureFromRawMemory(
      const scoped_refptr<ConstRawTextureMemory>& raw_texture_memory,
      intptr_t offset, const SurfaceInfo& surface_info,
      int pitch_in_bytes) OVERRIDE;

  scoped_refptr<RenderTarget> CreateOffscreenRenderTarget(
      const math::Size& dimensions) OVERRIDE;
  scoped_ptr<Texture> CreateTextureFromOffscreenRenderTarget(
      const scoped_refptr<RenderTarget>& render_target) OVERRIDE;
  scoped_array<uint8_t> GetCopyOfTexturePixelDataAsRGBA(
      const Texture& texture) OVERRIDE;

  SbBlitterContext GetSbBlitterContext() const { return context_; }

 private:
  SbBlitterDevice device_;
  SbBlitterContext context_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_
