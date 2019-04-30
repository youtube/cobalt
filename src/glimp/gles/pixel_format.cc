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

#include "glimp/gles/pixel_format.h"

namespace glimp {
namespace gles {

namespace {

// Must match the ordering of the PixelFormat enum defined in pixel_format.h.
const int kBytesPerPixel[] = {
    4,  // kPixelFormatRGBA8
    4,  // kPixelFormatARGB8
    4,  // kPixelFormatBGRA8
    2,  // kPixelFormatRGB565
    2,  // kPixelFormatBA8
    1,  // kPixelFormatA8
    2,  // kPixelFormatA16
};

}  // namespace

int BytesPerPixel(PixelFormat format) {
  SB_COMPILE_ASSERT(SB_ARRAY_SIZE(kBytesPerPixel) == kPixelFormatNumFormats,
                    kBytesPerPixel_has_entries_for_each_enum_PixelFormat);
  if (kPixelFormatInvalid == format) {
    return 0;  // Invalid formats have 0 bytes per pixel.
  }

  if (format < 0 || format >= kPixelFormatNumFormats) {
    SB_DCHECK(false) << "Invalid PixelFormat: " << format;
    return 0;
  }
  return kBytesPerPixel[format];
}

}  // namespace gles
}  // namespace glimp
