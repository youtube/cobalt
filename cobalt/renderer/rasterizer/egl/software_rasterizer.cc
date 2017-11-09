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

#include "cobalt/renderer/rasterizer/egl/software_rasterizer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

SoftwareRasterizer::SoftwareRasterizer(
    backend::GraphicsContext* context, int surface_cache_size,
    bool purge_skia_font_caches_on_destruction)
    : context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(context)),
      skia_rasterizer_(surface_cache_size,
                       purge_skia_font_caches_on_destruction) {}

void SoftwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  int width = render_target->GetSize().width();
  int height = render_target->GetSize().height();

  // Determine the image size and format that we will use to render to.
  SkImageInfo output_image_info =
      SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType);

  // Allocate the pixels for the output image.
  GLenum gl_format = GL_INVALID_ENUM;
  switch (output_image_info.colorType()) {
    case kRGBA_8888_SkColorType: {
      gl_format = GL_RGBA;
      break;
    }
    case kBGRA_8888_SkColorType: {
      gl_format = GL_BGRA_EXT;
      break;
    }
    default: { NOTREACHED() << "Unsupported GL color format."; }
  }
  scoped_ptr<cobalt::renderer::backend::TextureDataEGL> bitmap_pixels =
      context_->system_egl()->AllocateTextureData(
          math::Size(output_image_info.width(), output_image_info.height()),
          gl_format);

  SkBitmap bitmap;
  bitmap.installPixels(output_image_info, bitmap_pixels->GetMemory(),
                       bitmap_pixels->GetPitchInBytes());

  // Setup our Skia canvas that we will be using as the target for all CPU Skia
  // output.
  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  skia_rasterizer_.Submit(render_tree, &canvas);

  // The rasterized pixels are still on the CPU, ship them off to the GPU
  // for output to the display.  We must first create a backend GPU texture
  // with the data so that it is visible to the GPU.
  scoped_ptr<backend::TextureEGL> output_texture =
      context_->CreateTexture(bitmap_pixels.Pass());

  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      context_, render_target_egl);

  context_->Blit(output_texture->gl_handle(), 0, 0, width, height);
  context_->SwapBuffers(render_target_egl);
}

render_tree::ResourceProvider* SoftwareRasterizer::GetResourceProvider() {
  return skia_rasterizer_.GetResourceProvider();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
