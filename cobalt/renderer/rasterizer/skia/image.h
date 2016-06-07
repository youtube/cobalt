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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_IMAGE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_IMAGE_H_

#include "cobalt/base/type_id.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Introduce a base class for both software and hardware Skia images.  They
// should both be able to return a SkBitmap that can be used in subsequent
// Skia draw calls.
class SkiaImage : public render_tree::Image {
 public:
  // Ensures that any queued backend initialization of this image object is
  // completed after this method returns.  This can only be called from the
  // rasterizer thread.  When an Image is created (from any thread), the
  // construction of its backend is message queued to be initialized on the
  // render thread, however it is possible (via animations) for the image
  // to be referenced in a render tree before then, and thus this function will
  // fast-track the initialization of the backend object if it has not yet
  // executed.
  virtual void EnsureInitialized() = 0;

  // Mechanism to allow dynamic type checking on SkiaImage objects.
  virtual base::TypeId GetTypeId() const = 0;

  // A helper function for DCHECKing that given image data is indeed in
  // premultiplied alpha format.  Note that because of the nature of
  // premultiplied alpha, it is possible for a non-premultiplied alpha image
  // to slip by these checks.
  static void DCheckForPremultipliedAlpha(const math::Size& dimensions,
                                          int source_pitch_in_bytes,
                                          render_tree::PixelFormat pixel_format,
                                          const uint8_t* source_pixels);
};

// A single-plane image is an image where all data to describe a single pixel
// is stored contiguously.  This style of image is by far the most common.
class SkiaSinglePlaneImage : public SkiaImage {
 public:
  virtual const SkBitmap& GetBitmap() const = 0;

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<SkiaSinglePlaneImage>();
  }
};

// A multi-plane image is one where different channels may have different planes
// (i.e. sub-images) stored in different regions of memory.  A multi-plane
// image can be defined in terms of a set of single-plane images.
class SkiaMultiPlaneImage : public SkiaImage {
 public:
  virtual render_tree::MultiPlaneImageFormat GetFormat() const = 0;
  virtual const SkBitmap& GetBitmap(int plane_index) const = 0;

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<SkiaMultiPlaneImage>();
  }
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_IMAGE_H_
