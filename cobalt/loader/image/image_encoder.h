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

#ifndef COBALT_LOADER_IMAGE_IMAGE_ENCODER_H_
#define COBALT_LOADER_IMAGE_IMAGE_ENCODER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace loader {
namespace image {

class EncodedStaticImage : public Image {
 public:
  enum class ImageFormat {
    kPNG,
    kWEBP,
  };

  EncodedStaticImage(ImageFormat image_format, scoped_array<uint8> memory,
                     uint32 size_in_bytes, const math::Size& image_dimensions);

  ImageFormat GetImageFormat() const { return image_format_; }
  uint8_t* GetMemory() { return memory_.get(); }

  uint32 GetEstimatedSizeInBytes() const override { return size_in_bytes_; }
  const math::Size& GetSize() const override { return image_dimensions_; }
  bool IsOpaque() const override { return false; }
  bool IsAnimated() const override { return false; }

 private:
  ImageFormat image_format_;
  uint32 size_in_bytes_;
  math::Size image_dimensions_;
  scoped_array<uint8> memory_;
};

scoped_array<uint8> WriteRGBAPixelsToPNG(const uint8* const pixel_data,
                                         const math::Size& dimensions,
                                         size_t* out_num_bytes);

scoped_refptr<EncodedStaticImage> CompressRGBAImage(
    EncodedStaticImage::ImageFormat desired_format,
    const uint8* const image_data, const math::Size& dimensions);

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_ENCODER_H_
