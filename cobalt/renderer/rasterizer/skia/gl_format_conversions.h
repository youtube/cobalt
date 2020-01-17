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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_GL_FORMAT_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_GL_FORMAT_CONVERSIONS_H_

#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/private/GrTypesPriv.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

GLenum ConvertRenderTreeFormatToGL(render_tree::PixelFormat pixel_format);
GrColorType ConvertGLFormatToGrColorType(GLenum gl_format);
// Converts a base internal format to the sized internal format that a Skia
// backend texture expects.
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
GLenum ConvertBaseGLFormatToSizedInternalFormat(GLenum gl_format);

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_GL_FORMAT_CONVERSIONS_H_
