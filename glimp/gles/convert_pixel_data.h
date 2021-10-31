/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_CONVERT_PIXEL_DATA_H_
#define GLIMP_GLES_CONVERT_PIXEL_DATA_H_

#include "glimp/gles/pixel_format.h"

namespace glimp {
namespace gles {

enum PixelDataEndian {
  kPixelDataLittleEndian,
  kPixelDataBigEndian,
};

// Converts pixel data in pixel buffer from the source format into the
// destination format, swizzeling the pixel color components if necessary.
void ConvertPixelDataInplace(uint8_t* pixels,
                             int pitch_in_bytes,
                             PixelFormat destination_format,
                             PixelFormat source_format,
                             int width,
                             int height);

// Copies pixel data from the source buffer and format into the destination
// buffer and format, swizzeling the pixel color components if necessary.
void ConvertPixelData(uint8_t* destination,
                      int destination_pitch_in_bytes,
                      PixelFormat destination_format,
                      const uint8_t* source,
                      int source_pitch_in_bytes,
                      PixelFormat source_format,
                      int width,
                      int height,
                      bool flip_y);

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_CONVERT_PIXEL_DATA_H_
