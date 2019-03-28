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

#ifndef GLIMP_GLES_PIXEL_FORMAT_H_
#define GLIMP_GLES_PIXEL_FORMAT_H_

#include "glimp/gles/buffer_impl.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

// An internal structure which maintains information on a limited number of
// glimp-supported pixel formats for both textures and render targets.
enum PixelFormat {
  kPixelFormatRGBA8,
  kPixelFormatARGB8,
  kPixelFormatBGRA8,
  kPixelFormatRGB565,
  kPixelFormatBA8,
  kPixelFormatA8,
  kPixelFormatA16,
  kPixelFormatInvalid,
  kPixelFormatNumFormats = kPixelFormatInvalid
};

// Returns the number of bytes per pixel for a given PixelFormat.
int BytesPerPixel(PixelFormat format);

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PIXEL_FORMAT_H_
