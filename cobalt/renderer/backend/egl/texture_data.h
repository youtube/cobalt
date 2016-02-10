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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_

#include <GLES2/gl2.h>

#include "cobalt/renderer/backend/surface_info.h"
#include "cobalt/renderer/backend/texture.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextEGL;

// All TextureDataEGL subclasses must also implement ConvertToTexture() to
// convert their pixel buffer data to a GL texture object and return the handle
// to it.  After this method is called, the TextureData should be considered
// invalid.
class TextureDataEGL : public TextureData {
 public:
  virtual GLuint ConvertToTexture(GraphicsContextEGL* graphics_context,
                                  bool bgra_supported) = 0;
};

// Similar to TextureDataEGL::ConvertToTexture(), though this creates a texture
// without invalidating the RawTextureMemory object, so that multiple textures
// may be created through this data.
class RawTextureMemoryEGL : public RawTextureMemory {
 public:
  virtual GLuint CreateTexture(GraphicsContextEGL* graphics_context,
                               intptr_t offset, const SurfaceInfo& surface_info,
                               int pitch_in_bytes,
                               bool bgra_supported) const = 0;
};

void SetupInitialTextureParameters();
GLenum SurfaceInfoFormatToGL(SurfaceInfo::Format format);

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_
