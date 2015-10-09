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

#ifndef LOADER_IMAGE_PNG_IMAGE_DECODER_H_
#define LOADER_IMAGE_PNG_IMAGE_DECODER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "third_party/libpng/png.h"

namespace cobalt {
namespace loader {
namespace image {

// TODO(***REMOVED***): support decoding PNG image with multiple chunks.
class PNGImageDecoder : public ImageDataDecoder {
 public:
  explicit PNGImageDecoder(render_tree::ResourceProvider* resource_provider);
  ~PNGImageDecoder() OVERRIDE;

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) OVERRIDE;
  void Finish() OVERRIDE;

  // From ImageDataDecoder
  std::string GetTypeString() const OVERRIDE { return "PNGImageDecoder"; }

 private:
  static void HeaderAvailable(png_structp png, png_infop);
  static void RowAvailable(png_structp png, png_bytep row_buffer,
                           png_uint_32 row_index, int interlace_pass);

  void HeaderAvailableCallback();
  void RowAvailableCallback(png_bytep row_buffer, png_uint_32 row_index);

  void AllocateImageData(const math::Size& size);

  png_structp png_;
  png_infop info_;
  bool has_alpha_;
  png_bytep interlace_buffer_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_IMAGE_PNG_IMAGE_DECODER_H_
