// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/loader/image/image_encoder.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/renderer/test/jpeg_utils/jpeg_encode.h"
#include "cobalt/renderer/test/png_utils/png_encode.h"
#include "third_party/libwebp/src/webp/encode.h"

namespace cobalt {
namespace loader {
namespace image {

scoped_refptr<loader::image::EncodedStaticImage> CompressRGBAImage(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const uint8* const image_data, const math::Size& dimensions) {
  TRACE_EVENT0("cobalt::loader", "ImageEncoder::CompressRGBAImage()");

  const int kRGBABytesPerPixel = 4;
  const int kPitchSizeInBytes = dimensions.width() * kRGBABytesPerPixel;
  size_t num_bytes = 0;
  std::unique_ptr<uint8[]> compressed_data;

  using ImageFormat = loader::image::EncodedStaticImage::ImageFormat;
  switch (desired_format) {
    case ImageFormat::kPNG: {
      compressed_data = renderer::test::png_utils::EncodeRGBAToBuffer(
          image_data, dimensions.width(), dimensions.height(),
          kPitchSizeInBytes, &num_bytes);
      break;
    }

#if !defined(COBALT_BUILD_TYPE_GOLD)
    case ImageFormat::kJPEG: {
      compressed_data = renderer::test::jpeg_utils::EncodeRGBAToBuffer(
          image_data, dimensions.width(), dimensions.height(),
          kPitchSizeInBytes, &num_bytes);
      break;
    }
#endif

    case ImageFormat::kWEBP:
      NOTIMPLEMENTED();
      return nullptr;
  }

  return scoped_refptr<loader::image::EncodedStaticImage>(
      new loader::image::EncodedStaticImage(
          desired_format, std::move(compressed_data),
          static_cast<int>(num_bytes), dimensions));
}

EncodedStaticImage::EncodedStaticImage(
    image::EncodedStaticImage::ImageFormat image_format,
    std::unique_ptr<uint8[]> memory, uint32 size_in_bytes,
    const math::Size& image_dimensions)
    : image_format_(image_format),
      size_in_bytes_(size_in_bytes),
      image_dimensions_(image_dimensions),
      memory_(std::move(memory)) {}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
