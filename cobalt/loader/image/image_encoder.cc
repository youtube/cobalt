// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/image_encoder.h"

#include "cobalt/renderer/test/png_utils/png_encode.h"
#include "third_party/libwebp/webp/encode.h"

namespace cobalt {
namespace loader {
namespace image {

using cobalt::renderer::test::png_utils::EncodeRGBAToBuffer;

scoped_array<uint8> WriteRGBAPixelsToPNG(const uint8* const pixel_data,
                                         const math::Size& dimensions,
                                         size_t* out_num_bytes) {
  const int kRGBABytesPerPixel = 4;
  const int kPitchSizeInBytes = dimensions.width() * kRGBABytesPerPixel;
  return EncodeRGBAToBuffer(pixel_data, dimensions.width(), dimensions.height(),
                            kPitchSizeInBytes, out_num_bytes);
}

scoped_refptr<loader::image::EncodedStaticImage> CompressRGBAImage(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const uint8* const image_data, const math::Size& dimensions) {
  using ImageFormat = loader::image::EncodedStaticImage::ImageFormat;
  switch (desired_format) {
    case ImageFormat::kPNG: {
      size_t num_bytes;
      scoped_array<uint8> png_data =
          WriteRGBAPixelsToPNG(image_data, dimensions, &num_bytes);
      DCHECK_LT(static_cast<int>(num_bytes), kint32max);

      return scoped_refptr<loader::image::EncodedStaticImage>(
          new loader::image::EncodedStaticImage(desired_format, png_data.Pass(),
                                                static_cast<int>(num_bytes),
                                                dimensions));
    }
    case ImageFormat::kWEBP:
      NOTIMPLEMENTED();
  }

  return nullptr;
}

EncodedStaticImage::EncodedStaticImage(
    image::EncodedStaticImage::ImageFormat image_format,
    scoped_array<uint8> memory, uint32 size_in_bytes,
    const math::Size& image_dimensions)
    : image_format_(image_format),
      size_in_bytes_(size_in_bytes),
      image_dimensions_(image_dimensions),
      memory_(memory.Pass()) {}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
