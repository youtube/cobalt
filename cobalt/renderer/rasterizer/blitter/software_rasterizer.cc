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

#include "cobalt/renderer/rasterizer/blitter/software_rasterizer.h"

#if SB_HAS(BLITTER)

#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "starboard/blitter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

SoftwareRasterizer::SoftwareRasterizer(
    backend::GraphicsContext* context,
    bool purge_skia_font_caches_on_destruction)
    : context_(base::polymorphic_downcast<backend::GraphicsContextBlitter*>(
          context)),
      skia_rasterizer_(purge_skia_font_caches_on_destruction) {}

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
  SbBlitterPixelDataFormat blitter_pixel_data_format =
      SkiaToBlitterPixelFormat(output_image_info.colorType());

  CHECK(SbBlitterIsPixelFormatSupportedByPixelData(
      context_->GetSbBlitterDevice(), blitter_pixel_data_format));
  SbBlitterPixelData pixel_data = SbBlitterCreatePixelData(
      context_->GetSbBlitterDevice(), width, height, blitter_pixel_data_format);
  CHECK(SbBlitterIsPixelDataValid(pixel_data));

  SkBitmap bitmap;
  bitmap.installPixels(output_image_info,
                       SbBlitterGetPixelDataPointer(pixel_data),
                       SbBlitterGetPixelDataPitchInBytes(pixel_data));

  // Setup our Skia canvas that we will be using as the target for all CPU Skia
  // output.
  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  skia_rasterizer_.Submit(render_tree, &canvas);

  // The rasterized pixels are still on the CPU, ship them off to the GPU
  // for output to the display.  We must first create a backend GPU texture
  // with the data so that it is visible to the GPU.
  SbBlitterSurface surface = SbBlitterCreateSurfaceFromPixelData(
      context_->GetSbBlitterDevice(), pixel_data);

  backend::RenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<backend::RenderTargetBlitter*>(
          render_target.get());

  SbBlitterContext context = context_->GetSbBlitterContext();

  SbBlitterSetRenderTarget(context, render_target_blitter->GetSbRenderTarget());

  SbBlitterSetBlending(context, false);
  SbBlitterSetModulateBlitsWithColor(context, false);

  SbBlitterBlitRectToRect(context, surface,
                          SbBlitterMakeRect(0, 0, width, height),
                          SbBlitterMakeRect(0, 0, width, height));

  SbBlitterFlushContext(context);

  SbBlitterDestroySurface(surface);

  render_target_blitter->Flip();
}

render_tree::ResourceProvider* SoftwareRasterizer::GetResourceProvider() {
  return skia_rasterizer_.GetResourceProvider();
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_HAS(BLITTER)
