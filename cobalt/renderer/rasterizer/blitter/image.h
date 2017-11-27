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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_IMAGE_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_IMAGE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

typedef base::Callback<void(const scoped_refptr<render_tree::Node>& render_tree,
                            const scoped_refptr<backend::RenderTarget>&
                                render_target)> SubmitOffscreenCallback;

// render_tree::ImageData objects are implemented in the Blitter API via the
// SbBlitterPixelData objects, which is conceptually an exact match for
// ImageData.
class ImageData : public render_tree::ImageData {
 public:
  ImageData(SbBlitterDevice device, const math::Size& size,
            render_tree::PixelFormat pixel_format,
            render_tree::AlphaFormat alpha_format);
  ~ImageData() override;

  const render_tree::ImageDataDescriptor& GetDescriptor() const override {
    return *descriptor_;
  }

  uint8* GetMemory() override;

  SbBlitterPixelData TakePixelData();
  SbBlitterDevice device() { return device_; }

 private:
  SbBlitterDevice device_;
  SbBlitterPixelData pixel_data_;
  base::optional<render_tree::ImageDataDescriptor> descriptor_;
};

// render_tree::Image objects are implemented in the Blitter API via
// SbBlitterSurface objects, which are conceptually an exact match for Image.
// We derive from skia::SinglePlaneImage here because it is possible for
// render tree nodes referencing blitter:Images to be passed into a Skia
// software renderer.  Thus, SinglePlaneImage is implemented such
// that Skia render tree node visitors can also render it.
class SinglePlaneImage : public skia::SinglePlaneImage {
 public:
  explicit SinglePlaneImage(scoped_ptr<ImageData> image_data);
  // If |delete_function| is provided, it will be called in the destructor
  // instead of manually calling SbBlitterDestroySurface(surface_).
  SinglePlaneImage(SbBlitterSurface surface, bool is_opaque,
                   const base::Closure& delete_function);

  SinglePlaneImage(const scoped_refptr<render_tree::Node>& root,
                   SubmitOffscreenCallback submit_offscreen_callback,
                   SbBlitterDevice device);

  const math::Size& GetSize() const override { return size_; }

  SbBlitterSurface surface() const { return surface_; }

  // Overrides from skia::SinglePlaneImage.
  bool EnsureInitialized() override;

  // When GetBitmap() is called on a blitter::SinglePlaneImage for the first
  // time, we do a one-time download of the pixel data from the Blitter API
  // surface into a SkBitmap.
  const SkBitmap* GetBitmap() const override;

  bool IsOpaque() const override { return is_opaque_; }

 private:
  ~SinglePlaneImage() override;

  void InitializeImageFromRenderTree(
      const scoped_refptr<render_tree::Node>& root,
      const SubmitOffscreenCallback& submit_offscreen_callback,
      SbBlitterDevice device);

  math::Size size_;
  SbBlitterSurface surface_;

  bool is_opaque_;

  // This field is populated when GetBitmap() is called for the first time, and
  // after that is never modified.
  mutable base::optional<SkBitmap> bitmap_;

  // If |delete_function| is provided, it will be called in the destructor
  // instead of manually calling SbBlitterDestroySurface(surface_).
  base::Closure delete_function_;

  // This closure binds the image construction parameters so that we can delay
  // construction of it until it is accessed by the rasterizer thread.
  base::Closure initialize_image_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_IMAGE_H_
