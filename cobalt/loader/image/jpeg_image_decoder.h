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

#ifndef COBALT_LOADER_IMAGE_JPEG_IMAGE_DECODER_H_
#define COBALT_LOADER_IMAGE_JPEG_IMAGE_DECODER_H_

#include <setjmp.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_data_decoder.h"
#include "starboard/file.h"

// Inhibit C++ name-mangling for libjpeg functions.
extern "C" {
#include "third_party/libjpeg-turbo/jpeglib.h"
}

namespace cobalt {
namespace loader {
namespace image {

class JPEGImageDecoder : public ImageDataDecoder {
 public:
  // Pass true to |allow_image_decoding_to_multi_plane| to allow the decoder to
  // produce output in YUV  whenever possible, which saves both decoding time
  // and memory footprint.  Pass false to it on platforms that cannot render
  // multi plane images efficiently, and the output will always be produced in
  // single plane RGBA or BGRA.
  JPEGImageDecoder(render_tree::ResourceProvider* resource_provider,
                   const base::DebuggerHooks& debugger_hooks,
                   bool allow_image_decoding_to_multi_plane);
  ~JPEGImageDecoder() override;

  // From ImageDataDecoder
  std::string GetTypeString() const override { return "JPEGImageDecoder"; }

 private:
  enum OutputFormat {
    kOutputFormatInvalid,
    kOutputFormatYUV,
    kOutputFormatRGBA,
    kOutputFormatBGRA,
  };

  // From ImageDataDecoder
  size_t DecodeChunkInternal(const uint8* data, size_t size) override;
  scoped_refptr<Image> FinishInternal() override;

  bool ReadHeader();
  bool StartDecompress();
  bool DecodeProgressiveJPEG();
  bool ReadYUVLines();
  bool ReadRgbaOrGbraLines();
  bool ReadLines();

  const bool allow_image_decoding_to_multi_plane_;

  jpeg_decompress_struct info_;
  jpeg_source_mgr source_manager_;
  jpeg_error_mgr error_manager_;

  OutputFormat output_format_ = kOutputFormatInvalid;

  // This is only used when |output_format_| is kOutputFormatRGBA or
  // kOutputFormatBGRA.
  std::unique_ptr<render_tree::ImageData> decoded_image_data_;

  // All the following variables are only valid when |output_format_| is
  // kOutputFormatYUV.
  std::unique_ptr<render_tree::RawImageMemory> raw_image_memory_;

  // Sample factors of y plane, they are 1 for yuv444 or 2 for yuv420 but may
  // have other value combinations for other yuv formats.
  int h_sample_factor_ = 0;
  int v_sample_factor_ = 0;
  // Width/height of y plane, aligned to |kDctScaleSize| * |y_sample_factor_|.
  JDIMENSION y_plane_width_ = 0;
  JDIMENSION y_plane_height_ = 0;
};

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_IMAGE_JPEG_IMAGE_DECODER_H_
