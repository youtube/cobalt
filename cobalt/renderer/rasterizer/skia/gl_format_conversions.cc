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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"

#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

GLenum ConvertRenderTreeFormatToGL(render_tree::PixelFormat pixel_format) {
  switch (pixel_format) {
    case render_tree::kPixelFormatRGBA8:
    case render_tree::kPixelFormatUYVY:
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
    case render_tree::kPixelFormatUV8:
      return GL_LUMINANCE_ALPHA;
    default: { NOTREACHED() << "Unknown format."; }
  }
  return GL_RGBA;
}

GrColorType ConvertGLFormatToGrColorType(GLenum gl_format) {
  switch (gl_format) {
    case GL_RGBA:
      return GrColorType::kRGBA_8888;
    case GL_BGRA_EXT:
      return GrColorType::kBGRA_8888;
    // Note GL_ALPHA and GL_RED_EXT probably don't really work.
    // They're only for use w/ SbDecodeTargets that are never drawn via skia
    case GL_ALPHA:
#if defined(GL_RED_EXT)
    case GL_RED_EXT:
#endif
      return GrColorType::kAlpha_8;
    // Note GL_LUMINANCE_ALPHA and GL_RG_EXT probably don't really work.
    // They're only for use w/ SbDecodeTargets that are never drawn via skia
    case GL_LUMINANCE_ALPHA:
#if defined(GL_RG_EXT)
    case GL_RG_EXT:
#endif
      return GrColorType::kRGBA_8888;
    default: { NOTREACHED() << "Unsupported GL format."; }
  }
  return GrColorType::kRGBA_8888;
}

GLenum ConvertBaseGLFormatToSizedInternalFormat(GLenum gl_format) {
  switch (gl_format) {
    case GL_RGBA:
      // GL_RGBA8 is only defined in OpenGL ES 3. Use the GL_RGBA8_OES define
      // which is an extension available in OpenGL ES 2.
      return GL_RGBA8_OES;
    case GL_BGRA_EXT:
      return GL_BGRA8_EXT;
    // Note GL_ALPHA and GL_RED_EXT probably don't really work.
    // They're only for use w/ SbDecodeTargets that are never drawn via skia
    case GL_ALPHA:
#if defined(GL_RED_EXT)
    case GL_RED_EXT:
#endif
      return GL_ALPHA8_EXT;
    // Note GL_LUMINANCE_ALPHA and GL_RG_EXT probably don't really work.
    // They're only for use w/ SbDecodeTargets that are never drawn via skia
    case GL_LUMINANCE_ALPHA:
#if defined(GL_RG_EXT)
    case GL_RG_EXT:
#endif
      return GL_RGBA8_OES;
    default: { NOTREACHED() << "Unsupported GL format."; }
  }
  return GL_RGBA8_OES;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
