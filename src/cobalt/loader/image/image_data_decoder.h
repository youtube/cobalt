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

#ifndef COBALT_LOADER_IMAGE_IMAGE_DATA_DECODER_H_
#define COBALT_LOADER_IMAGE_IMAGE_DATA_DECODER_H_

#include <string>
#include <vector>

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
class ImageDataDecoder {
 public:
  explicit ImageDataDecoder(render_tree::ResourceProvider* resource_provider);

  virtual ~ImageDataDecoder() {}

  virtual std::string GetTypeString() const = 0;

  scoped_ptr<render_tree::ImageData> RetrieveImageData() {
    return image_data_.Pass();
  }

  void DecodeChunk(const uint8* data, size_t size);
  // Return true if decoding succeeded.
  bool FinishWithSuccess();

 protected:
  enum State {
    kWaitingForHeader,
    kReadLines,
    kDone,
    kError,
  };

  // Every subclass of ImageDataDecoder should override this function to do
  // the internal decoding. The return value of this function is the number of
  // decoded data.
  virtual size_t DecodeChunkInternal(const uint8* data, size_t input_byte) = 0;
  // Subclass can override this function to get a last chance to do some work.
  virtual void FinishInternal() {}

  void AllocateImageData(const math::Size& size);

  render_tree::ImageData* image_data() const { return image_data_.get(); }

  void set_state(State state) { state_ = state; }
  State state() const { return state_; }

  render_tree::PixelFormat pixel_format() const { return pixel_format_; }

 private:
  // Called on construction to query the ResourceProvider for the best image
  // pixel format to use.
  void CalculatePixelFormat();

  // |resource_provider_| is used to allocate render_tree::ImageData
  render_tree::ResourceProvider* const resource_provider_;
  // Decoded image data.
  scoped_ptr<render_tree::ImageData> image_data_;
  // |data_buffer_| is used to cache the undecoded data.
  std::vector<uint8> data_buffer_;
  // Record the current decoding status.
  State state_;
  // The pixel format that the decoded image data is expected to be in.
  render_tree::PixelFormat pixel_format_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_IMAGE_DATA_DECODER_H_
