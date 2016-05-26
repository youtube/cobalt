/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_
#define COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/copy_image_data.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/pixel_data_stub.h"
#include "cobalt/renderer/backend/render_target_stub.h"
#include "cobalt/renderer/backend/texture_stub.h"

namespace cobalt {
namespace renderer {
namespace backend {

// An implementation of GraphicsContext for the stub graphics system.
// Most methods are simply stubbed out and so this object does very little
// besides fight off compiler errors.
class GraphicsContextStub : public GraphicsContext {
 public:
  explicit GraphicsContextStub(GraphicsSystem* system)
      : GraphicsContext(system) {}

  scoped_ptr<Texture> CreateTexture(
      scoped_ptr<TextureData> texture_data) OVERRIDE {
    TextureDataStub* texture_data_stub =
        base::polymorphic_downcast<TextureDataStub*>(texture_data.get());

    return scoped_ptr<Texture>(new TextureStub(
        texture_data_stub->pixel_data()));
  }

  scoped_ptr<Texture> CreateTextureFromRawMemory(
      const scoped_refptr<ConstRawTextureMemory>& raw_texture_memory,
      intptr_t offset, const SurfaceInfo& surface_info,
      int pitch_in_bytes) OVERRIDE {
    return scoped_ptr<Texture>(new TextureStub(raw_texture_memory, offset,
                                               surface_info, pitch_in_bytes));
  }

  scoped_refptr<RenderTarget> CreateOffscreenRenderTarget(
      const math::Size& dimensions) OVERRIDE {
    return scoped_refptr<RenderTarget>(new RenderTargetStub(
        SurfaceInfo(dimensions, SurfaceInfo::kFormatRGBA8)));
  }
  scoped_ptr<Texture> CreateTextureFromOffscreenRenderTarget(
      const scoped_refptr<RenderTarget>& render_target) OVERRIDE {
    RenderTargetStub* render_target_stub =
        base::polymorphic_downcast<RenderTargetStub*>(render_target.get());

    return scoped_ptr<Texture>(
        new TextureStub(render_target_stub->pixel_data()));
  }
  scoped_array<uint8_t> GetCopyOfTexturePixelDataAsRGBA(
      const Texture& texture) OVERRIDE {
    const TextureStub* texture_stub =
        base::polymorphic_downcast<const TextureStub*>(&texture);
    const scoped_refptr<PixelDataStub>& pixel_data =
        texture_stub->pixel_data();

    const SurfaceInfo& surface_info = pixel_data->surface_info();
    size_t pixel_memory_size = surface_info.size.width() *
                               surface_info.size.height() *
                               SurfaceInfo::BytesPerPixel(surface_info.format);

    scoped_array<uint8_t> return_pixels(new uint8_t[pixel_memory_size]);
    memcpy(return_pixels.get(), pixel_data->memory(), pixel_memory_size);

    return return_pixels.Pass();
  }
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_
