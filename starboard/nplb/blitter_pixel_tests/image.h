// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_BLITTER_PIXEL_TESTS_IMAGE_H_
#define STARBOARD_NPLB_BLITTER_PIXEL_TESTS_IMAGE_H_

#include <string>

#include "starboard/blitter.h"
#include "starboard/file.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

// The Image class provides image-related functionality required
// by the Starboard Blitter API pixel tests.  In particular, it provides
// code to read/write images from/to PNGs, code for loading an image from an
// SbBlitterSurface, code for applying a Gaussian blur to an image, and code for
// pixel testing two different images.
class Image {
 public:
  // Download image data from a Starboard Blitter API surface.
  explicit Image(SbBlitterSurface surface);
  // Constructor allowing one to explicitly pass in the image data.  Note that
  // Image will take ownership of |passed_pixel_data|.
  Image(uint8_t* passed_pixel_data, int width, int height);
  // Loads image data from a PNG file located at |png_path|.
  explicit Image(const std::string& png_path);
  ~Image();

  // Tests to see if a file can be opened, in the same way that the constructor
  // that reads from a PNG file will open that PNG file.
  static bool CanOpenFile(const std::string& path);

  // Performs a diff of two images.  |pixel_test_value_fuzz| gives the distance
  // that a pixel color component must differ before being considered a
  // different value.  Setting |pixel_test_value_fuzz| to 0 will make the diff
  // test strict.  |is_same| will be set to true if there are no differing
  // pixels and false if any pixel differs.
  Image Diff(const Image& other,
             int pixel_test_value_fuzz,
             bool* is_same) const;

  // Returns a new image that is a blurred version of the original.  The blur
  // is performed by convolving the original image with a Gaussian function
  // whose standard deviation is specified by |sigma|, in units of pixels.
  Image GaussianBlur(float sigma) const;

  // Saves the contents of this image to a PNG file located at |png_path|.
  void WriteToPNG(const std::string& png_path) const;

 private:
  static SbFile OpenFileForReading(const std::string& path);

  // Pixel data stored as 32-bit RGBA (byte-order).
  uint8_t* pixel_data_;
  int width_;
  int height_;
};

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)

#endif  // STARBOARD_NPLB_BLITTER_PIXEL_TESTS_IMAGE_H_
