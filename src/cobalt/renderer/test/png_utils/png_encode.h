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

#ifndef COBALT_RENDERER_TEST_PNG_UTILS_PNG_ENCODE_H_
#define COBALT_RENDERER_TEST_PNG_UTILS_PNG_ENCODE_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace png_utils {

// Encodes RGBA8 formatted pixel data to a PNG file on disk.
void EncodeRGBAToPNG(const FilePath& png_file_path, const uint8_t* pixel_data,
                     int width, int height, int pitch_in_bytes);

// Encodes RGBA8 formatted pixel data to an in memory buffer.
scoped_array<uint8> EncodeRGBAToBuffer(const uint8_t* pixel_data, int width,
                                       int height, int pitch_in_bytes,
                                       size_t* out_size);

}  // namespace png_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_TEST_PNG_UTILS_PNG_ENCODE_H_
