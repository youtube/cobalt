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

#ifndef LOADER_IMAGE_JPEG_IMAGE_DECODER_H_
#define LOADER_IMAGE_JPEG_IMAGE_DECODER_H_

#include <setjmp.h>

#include "base/callback.h"
#include "base/circular_buffer_shell.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/image/image_data_decoder.h"

// Inhibit C++ name-mangling for libjpeg functions.
extern "C" {
#include "third_party/libjpeg/jpeglib.h"
}

namespace cobalt {
namespace loader {
namespace image {

// TODO(***REMOVED***): support decoding JPEG image with multiple chunks.
class JPEGImageDecoder : public ImageDataDecoder {
 public:
  explicit JPEGImageDecoder(render_tree::ResourceProvider* resource_provider);
  ~JPEGImageDecoder() OVERRIDE;

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) OVERRIDE;
  void Finish() OVERRIDE;

  // From ImageDataDecoder
  std::string GetTypeString() const OVERRIDE { return "JPEGImageDecoder"; }

 private:
  enum State {
    kWaitingForHeader,
    kReadLines,
    kDone,
    kError,
  };

  bool ReadHeader();
  bool StartDecompress();
  bool DecodeProgressiveJPEG();
  bool ReadLines();

  void AllocateImageData(const math::Size& size);
  void CacheSourceBytesToBuffer();

  jpeg_decompress_struct info_;
  jpeg_source_mgr source_manager_;
  jpeg_error_mgr error_manager_;

  // Record the current decoding status.
  State state_;
  // |data_buffer_| is used to cache the undecoded data.
  scoped_ptr<base::CircularBufferShell> data_buffer_;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_IMAGE_JPEG_IMAGE_DECODER_H_
