// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_IMAGE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_IMAGE_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class SoftwareImageData : public render_tree::ImageData {
 public:
  SoftwareImageData(const math::Size& size,
                    render_tree::PixelFormat pixel_format,
                    render_tree::AlphaFormat alpha_format);

  const render_tree::ImageDataDescriptor& GetDescriptor() const override;
  uint8_t* GetMemory() override;

  scoped_array<uint8_t> PassPixelData();

 private:
  render_tree::ImageDataDescriptor descriptor_;
  scoped_array<uint8_t> pixel_data_;
};

class SoftwareImage : public SinglePlaneImage {
 public:
  explicit SoftwareImage(scoped_ptr<SoftwareImageData> source_data);
  SoftwareImage(uint8_t* source_data,
                const render_tree::ImageDataDescriptor& descriptor);

  const math::Size& GetSize() const override { return size_; }

  const SkBitmap* GetBitmap() const override { return &bitmap_; }

  bool EnsureInitialized() override { return false; }

  bool IsOpaque() const override { return is_opaque_; }

 private:
  void Initialize(uint8_t* source_data,
                  const render_tree::ImageDataDescriptor& descriptor);

  scoped_array<uint8_t> owned_pixel_data_;
  SkBitmap bitmap_;
  math::Size size_;
  bool is_opaque_;
};

class SoftwareRawImageMemory : public render_tree::RawImageMemory {
 public:
  SoftwareRawImageMemory(size_t size_in_bytes, size_t alignment);

  size_t GetSizeInBytes() const override;
  uint8_t* GetMemory() override;

  scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> PassPixelData();

 private:
  size_t size_in_bytes_;
  scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> pixel_data_;
};

class SoftwareMultiPlaneImage : public MultiPlaneImage {
 public:
  SoftwareMultiPlaneImage(
      scoped_ptr<SoftwareRawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor);

  const math::Size& GetSize() const override { return size_; }

  render_tree::MultiPlaneImageFormat GetFormat() const override {
    return format_;
  }
  const SkBitmap* GetBitmap(int plane_index) const override {
    return planes_[plane_index]->GetBitmap();
  }

  bool EnsureInitialized() override { return false; }

  // Currently, all supported multiplane images (e.g. mostly YUV) do not
  // support alpha, so multiplane images will always be opaque.
  bool IsOpaque() const override { return true; }

 private:
  math::Size size_;
  render_tree::MultiPlaneImageFormat format_;

  scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> owned_pixel_data_;

  scoped_refptr<SoftwareImage>
      planes_[render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes];
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_IMAGE_H_
