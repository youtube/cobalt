// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_IMAGE_DATA_DECODER_H_
#define COBALT_LOADER_IMAGE_IMAGE_DATA_DECODER_H_

#include <memory>
#include <string>
#include <vector>

#include "cobalt/base/debugger_hooks.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/render_tree/resource_provider.h"
#if defined(STARBOARD)
#include "starboard/decode_target.h"
#endif

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
  explicit ImageDataDecoder(render_tree::ResourceProvider* resource_provider,
                            const base::DebuggerHooks& debugger_hooks);

  virtual ~ImageDataDecoder() {}

  virtual std::string GetTypeString() const = 0;

  void DecodeChunk(const uint8* data, size_t size);
  scoped_refptr<Image> FinishAndMaybeReturnImage();

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
  // Subclass has to override to finalize the decoding and return decoded image.
  virtual scoped_refptr<Image> FinishInternal() = 0;

  render_tree::ResourceProvider* resource_provider() {
    return resource_provider_;
  }

  const base::DebuggerHooks& debugger_hooks() { return debugger_hooks_; }

  void set_state(State state) { state_ = state; }
  State state() const { return state_; }

  render_tree::PixelFormat pixel_format() const { return pixel_format_; }

  // Helper functions used by derived classes to create image and various
  // objects to hold decoded image data.
  std::unique_ptr<render_tree::ImageData> AllocateImageData(
      const math::Size& size, bool has_alpha);
  scoped_refptr<Image> CreateStaticImage(
      std::unique_ptr<render_tree::ImageData> image_data);

 private:
  // Called on construction to query the ResourceProvider for the best image
  // pixel format to use.
  void CalculatePixelFormat();

  // |resource_provider_| is used to allocate render_tree::ImageData
  render_tree::ResourceProvider* const resource_provider_;
  // |debugger_hooks_| is used with CLOG to report errors to the WebDev.
  const base::DebuggerHooks& debugger_hooks_;
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
