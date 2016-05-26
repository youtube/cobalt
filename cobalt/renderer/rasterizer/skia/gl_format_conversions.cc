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

#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"

#include <GLES2/gl2ext.h>

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

GLenum ConvertRenderTreeFormatToGL(render_tree::PixelFormat pixel_format) {
  switch (pixel_format) {
    case render_tree::kPixelFormatRGBA8:
      return GL_RGBA;
    // Our Skia GL backend has no reason to distinguish between Y/U/V textures,
    // they can all be treated as 8-bit greyscale alpha channels, expressed by
    // GL_ALPHA.
    case render_tree::kPixelFormatY8:
      return GL_ALPHA;
    case render_tree::kPixelFormatU8:
      return GL_ALPHA;
    case render_tree::kPixelFormatV8:
      return GL_ALPHA;
    default: { NOTREACHED() << "Unknown format."; }
  }
  return GL_RGBA;
}

GrPixelConfig ConvertGLFormatToGr(GLenum gl_format) {
  switch (gl_format) {
    case GL_RGBA:
      return kRGBA_8888_GrPixelConfig;
    case GL_BGRA_EXT:
      return kBGRA_8888_GrPixelConfig;
    case GL_ALPHA:
      return kAlpha_8_GrPixelConfig;
    default: { NOTREACHED() << "Unsupported GL format."; }
  }
  return kRGBA_8888_GrPixelConfig;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
