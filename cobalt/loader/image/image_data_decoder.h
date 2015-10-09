/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef LOADER_IMAGE_IMAGE_DATA_DECODER_H_
#define LOADER_IMAGE_IMAGE_DATA_DECODER_H_

#include "cobalt/loader/decoder.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace image {

// This class is used as a component of ImageDecoder and is intended to be
// sub-classed by specific data decoders, such as JPEG decoders and PNG
// decoders. This class is intended to decode image data based on the type,
// but leaves the responsibility of handling callbacks and producing the final
// image to ImageDecoder.
class ImageDataDecoder : public Decoder {
 public:
  explicit ImageDataDecoder(render_tree::ResourceProvider* resource_provider)
      : resource_provider_(resource_provider) {}

  virtual ~ImageDataDecoder() {}

  virtual std::string GetTypeString() const = 0;

  scoped_ptr<render_tree::ImageData> RetrieveImageData() {
    return image_data_.Pass();
  }

 protected:
  // |resource_provider_| is used to allocate render_tree::ImageData
  render_tree::ResourceProvider* const resource_provider_;
  scoped_ptr<render_tree::ImageData> image_data_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_IMAGE_IMAGE_DATA_DECODER_H_
