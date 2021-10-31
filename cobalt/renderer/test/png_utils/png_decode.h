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

#ifndef COBALT_RENDERER_TEST_PNG_UTILS_PNG_DECODE_H_
#define COBALT_RENDERER_TEST_PNG_UTILS_PNG_DECODE_H_

#include <memory>

#include "base/files/file_path.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace png_utils {

// Decodes a PNG file as RGBA directly to CPU memory.  Accepts width and
// height as output parameters that must not be NULL.  Alpha format will
// be unpremultiplied.
std::unique_ptr<uint8_t[]> DecodePNGToRGBA(const base::FilePath& png_file_path,
                                           int* width, int* height);

// Same as above, however this function will return the results as
// premultiplied alpha format pixels.
std::unique_ptr<uint8_t[]> DecodePNGToPremultipliedAlphaRGBA(
    const base::FilePath& png_file_path, int* width, int* height);

// Creates a render_tree::Image object out of a PNG file that can be referenced
// within a render tree.
scoped_refptr<cobalt::render_tree::Image> DecodePNGToRenderTreeImage(
    const base::FilePath& png_file_path,
    render_tree::ResourceProvider* resource_provider);

}  // namespace png_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_TEST_PNG_UTILS_PNG_DECODE_H_
