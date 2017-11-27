// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_IMAGE_PNG_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_PNG_IMAGE_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "third_party/libpng/png.h"

namespace cobalt {
namespace loader {
namespace image {

class PNGImageDecoder : public ImageDataDecoder {
 public:
  explicit PNGImageDecoder(render_tree::ResourceProvider* resource_provider);
  ~PNGImageDecoder() override;

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "PNGImageDecoder"; }

 private:
  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t input_byte) override;

  // Callbacks which feed libpng.
  static void HeaderAvailable(png_structp png, png_infop info);
  static void RowAvailable(png_structp png, png_bytep row_buffer,
                           png_uint_32 row_index, int interlace_pass);
  static void DecodeDone(png_structp png, png_infop info);

  // Functions which are called by libpng Callbacks.
  void HeaderAvailableCallback();
  void RowAvailableCallback(png_bytep row_buffer, png_uint_32 row_index);
  void DecodeDoneCallback();

  png_structp png_;
  png_infop info_;
  bool has_alpha_;
  png_bytep interlace_buffer_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_PNG_IMAGE_DECODER_H_
