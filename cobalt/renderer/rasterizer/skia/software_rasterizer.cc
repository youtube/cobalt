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

#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

#include "base/debug/trace_event.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/skia/software_resource_provider.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SkiaSoftwareRasterizer::SkiaSoftwareRasterizer(
    backend::GraphicsContext* graphics_context)
    : graphics_context_(graphics_context),
      resource_provider_(new SkiaSoftwareResourceProvider()) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareRasterizer::SkiaSoftwareRasterizer()");
}

namespace {

class SoftwareScratchSurface
    : public SkiaRenderTreeNodeVisitor::ScratchSurface {
 public:
  explicit SoftwareScratchSurface(const math::Size& size)
      : surface_(SkSurface::NewRaster(
            SkImageInfo::MakeN32Premul(size.width(), size.height()))) {}
  SkSurface* GetSurface() OVERRIDE { return surface_.get(); }

 private:
  SkAutoTUnref<SkSurface> surface_;
};

scoped_ptr<SkiaRenderTreeNodeVisitor::ScratchSurface> CreateScratchSurface(
    const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "CreateScratchSurface()", "width",
               size.width(), "height", size.height());
  return scoped_ptr<SkiaRenderTreeNodeVisitor::ScratchSurface>(
      new SoftwareScratchSurface(size));
}

void ReturnScratchImage(SkSurface* surface) { surface->unref(); }

}  // namespace

void SkiaSoftwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");
  TRACE_EVENT0("cobalt::renderer", "SkiaSoftwareRasterizer::Submit()");

  // Determine the image size and format that we will use to render to.
  SkImageInfo output_image_info = SkImageInfo::MakeN32(
      render_target->GetSurfaceInfo().size.width(),
      render_target->GetSurfaceInfo().size.height(), kPremul_SkAlphaType);

  // Allocate the pixels for the output image.
  scoped_ptr<cobalt::renderer::backend::TextureData> bitmap_pixels =
      graphics_context_->system()->AllocateTextureData(backend::SurfaceInfo(
          math::Size(output_image_info.width(), output_image_info.height()),
          SkiaSurfaceFormatToCobalt(output_image_info.colorType())));

  SkBitmap bitmap;
  bitmap.installPixels(output_image_info, bitmap_pixels->GetMemory(),
                       bitmap_pixels->GetPitchInBytes());

  // Setup our Skia canvas that we will be using as the target for all CPU Skia
  // output.
  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    // Create the rasterizer and setup its render target to the bitmap we have
    // just created above.
    SkiaRenderTreeNodeVisitor::CreateScratchSurfaceFunction
        create_scratch_surface_function = base::Bind(&CreateScratchSurface);
    SkiaRenderTreeNodeVisitor visitor(&canvas,
                                      &create_scratch_surface_function);

    // Finally, rasterize the render tree to the output canvas using the
    // rasterizer we just created.
    render_tree->Accept(&visitor);
  }

  // The rasterized pixels are still on the CPU, ship them off to the GPU
  // for output to the display.  We must first create a backend GPU texture
  // with the data so that it is visible to the GPU.
  scoped_ptr<backend::Texture> output_texture =
      graphics_context_->CreateTexture(bitmap_pixels.Pass());

  // Attach the output render target to the graphics context so we can blit
  // the image to the target.
  scoped_ptr<backend::GraphicsContext::Frame> frame(
      graphics_context_->StartFrame(render_target));

  // Issue the command to render the texture to the display.
  frame->BlitToRenderTarget(*output_texture);
}

render_tree::ResourceProvider* SkiaSoftwareRasterizer::GetResourceProvider() {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaSoftwareRasterizer::GetResourceProvider()");
  return resource_provider_.get();
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
