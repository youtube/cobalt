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

#include "cobalt/renderer/backend/egl/texture_data_cpu.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/memory/aligned_memory.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {
GLuint UploadPixelDataToNewTexture(GraphicsContextEGL* graphics_context,
                                   const uint8_t* data,
                                   const SurfaceInfo& surface_info,
                                   int pitch_in_bytes, bool bgra_supported) {
  GLuint texture_handle;

  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context);

  GL_CALL(glGenTextures(1, &texture_handle));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
  SetupInitialTextureParameters();

  GLenum gl_format = SurfaceInfoFormatToGL(surface_info.format);
  if (gl_format == GL_BGRA_EXT) {
    DCHECK(bgra_supported);
  }

  DCHECK_EQ(surface_info.size.width() *
                SurfaceInfo::BytesPerPixel(surface_info.format),
            pitch_in_bytes);

  // Copy pixel data over from the user provided source data into the OpenGL
  // driver to be used as a texture from now on.
  GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, gl_format, surface_info.size.width(),
                       surface_info.size.height(), 0, gl_format,
                       GL_UNSIGNED_BYTE, data));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  return texture_handle;
}
}  // namespace

TextureDataCPU::TextureDataCPU(const SurfaceInfo& surface_info)
    : surface_info_(surface_info),
      memory_(new uint8_t[GetPitchInBytes() * surface_info_.size.height()]) {}

GLuint TextureDataCPU::ConvertToTexture(GraphicsContextEGL* graphics_context,
                                        bool bgra_supported) {
  GLuint ret = UploadPixelDataToNewTexture(graphics_context, GetMemory(),
                                           GetSurfaceInfo(), GetPitchInBytes(),
                                           bgra_supported);
  memory_.reset();

  return ret;
}

RawTextureMemoryCPU::RawTextureMemoryCPU(size_t size_in_bytes, size_t alignment)
    : size_in_bytes_(size_in_bytes) {
  memory_ = scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree>(
      static_cast<uint8_t*>(base::AlignedAlloc(size_in_bytes, alignment)));
}

GLuint RawTextureMemoryCPU::CreateTexture(GraphicsContextEGL* graphics_context,
                                          intptr_t offset,
                                          const SurfaceInfo& surface_info,
                                          int pitch_in_bytes,
                                          bool bgra_supported) const {
  return UploadPixelDataToNewTexture(graphics_context, memory_.get() + offset,
                                     surface_info, pitch_in_bytes,
                                     bgra_supported);
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
