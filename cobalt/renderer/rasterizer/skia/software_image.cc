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

#include "cobalt/renderer/rasterizer/skia/software_image.h"

#include "base/memory/aligned_memory.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SoftwareImageData::SoftwareImageData(const math::Size& size,
                                     render_tree::PixelFormat pixel_format,
                                     render_tree::AlphaFormat alpha_format)
    : descriptor_(size, pixel_format, alpha_format,
                  size.width() * render_tree::BytesPerPixel(pixel_format)),
      pixel_data_(new uint8_t[size.height() * descriptor_.pitch_in_bytes]) {}

const render_tree::ImageDataDescriptor& SoftwareImageData::GetDescriptor()
    const {
  return descriptor_;
}

uint8_t* SoftwareImageData::GetMemory() { return pixel_data_.get(); }

scoped_array<uint8_t> SoftwareImageData::PassPixelData() {
  return pixel_data_.Pass();
}

SoftwareImage::SoftwareImage(scoped_ptr<SoftwareImageData> source_data) {
  owned_pixel_data_ = source_data->PassPixelData();
  Initialize(owned_pixel_data_.get(), source_data->GetDescriptor());
}

SoftwareImage::SoftwareImage(
    uint8_t* source_data, const render_tree::ImageDataDescriptor& descriptor) {
  Initialize(source_data, descriptor);
}

namespace {
// Converts UV8 texture data to an ARGB SkBitmap where the original U channel is
// found in the B channel, the original V channel is found in the A channel, and
// the R and G channels are zeroed out.
void ConvertUV8ToARGBSkBitmap(
    uint8_t* source_data, const render_tree::ImageDataDescriptor& descriptor,
    SkBitmap* bitmap) {
  bitmap->allocN32Pixels(descriptor.size.width(), descriptor.size.height());
  int row_pixels_dest = bitmap->rowBytes() / bitmap->bytesPerPixel();

  uint8_t* current_src = source_data;
  SkColor* current_dest = static_cast<SkColor*>(bitmap->getPixels());
  for (int row = 0; row < descriptor.size.height(); ++row) {
    for (int column = 0; column < descriptor.size.width(); ++column) {
      current_dest[column] = SkColorSetARGBMacro(
          current_src[column * 2 + 1], 0, 0, current_src[column * 2 + 0]);
    }
    current_dest += row_pixels_dest;
    current_src += descriptor.pitch_in_bytes;
  }
}
}  // namespace

void SoftwareImage::Initialize(
    uint8_t* source_data, const render_tree::ImageDataDescriptor& descriptor) {
  is_opaque_ = (descriptor.alpha_format == render_tree::kAlphaFormatOpaque);
  SkAlphaType skia_alpha_format =
      RenderTreeAlphaFormatToSkia(descriptor.alpha_format);
  DCHECK_EQ(kPremul_SkAlphaType, skia_alpha_format);

  size_ = descriptor.size;

  if (descriptor.pixel_format == render_tree::kPixelFormatUV8) {
    // TODO: SKIA_M61_UNFINISHED_IMPLEMENATION: fix me
    // Convert UV8 to ARGB because Skia does not support any two-channel
    // formats.  This of course is not efficient, but efficiency in the software
    // renderer is not as important as completeness and correctness.
    // ConvertUV8ToARGBSkBitmap(source_data, descriptor, image_.get());
    NOTREACHED() << "Need to do this..";
  } else {
// Check that the incoming pixel data is indeed in premultiplied alpha
// format.
#if !defined(NDEBUG)
    Image::DCheckForPremultipliedAlpha(descriptor.size,
                                       descriptor.pitch_in_bytes,
                                       descriptor.pixel_format, source_data);
#endif
    // Convert our incoming pixel data from unpremultiplied alpha to
    // premultiplied alpha format, which is what Skia expects.
    SkImageInfo premul_image_info = SkImageInfo::Make(
        descriptor.size.width(), descriptor.size.height(),
        RenderTreeSurfaceFormatToSkia(descriptor.pixel_format),
        skia_alpha_format);
    SkBitmap bitmap;
    bitmap.installPixels(premul_image_info, source_data,
                         descriptor.pitch_in_bytes);
    image_ = SkImage::MakeFromBitmap(bitmap);
  }
}

SoftwareRawImageMemory::SoftwareRawImageMemory(size_t size_in_bytes,
                                               size_t alignment)
    : size_in_bytes_(size_in_bytes),
      pixel_data_(
          static_cast<uint8_t*>(base::AlignedAlloc(size_in_bytes, alignment))) {
}

size_t SoftwareRawImageMemory::GetSizeInBytes() const { return size_in_bytes_; }

uint8_t* SoftwareRawImageMemory::GetMemory() { return pixel_data_.get(); }

scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree>
SoftwareRawImageMemory::PassPixelData() {
  return pixel_data_.Pass();
}

SoftwareMultiPlaneImage::SoftwareMultiPlaneImage(
    scoped_ptr<SoftwareRawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor)
    : size_(descriptor.GetPlaneDescriptor(0).size),
      format_(descriptor.image_format()),
      owned_pixel_data_(raw_image_memory->PassPixelData()) {
  for (int i = 0; i < descriptor.num_planes(); ++i) {
    planes_[i] = new SoftwareImage(
        owned_pixel_data_.get() + descriptor.GetPlaneOffset(i),
        descriptor.GetPlaneDescriptor(i));
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
