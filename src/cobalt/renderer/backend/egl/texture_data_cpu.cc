// Copyright 2015 Google Inc. All Rights Reserved.
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
                                   const uint8_t* data, const math::Size& size,
                                   GLenum format, int pitch_in_bytes,
                                   bool bgra_supported) {
  GLuint texture_handle;

  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context);

  GL_CALL(glGenTextures(1, &texture_handle));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_handle));
  SetupInitialTextureParameters();

  if (format == GL_BGRA_EXT) {
    DCHECK(bgra_supported);
  }

  DCHECK_EQ(size.width() * BytesPerPixelForGLFormat(format), pitch_in_bytes);

  // Copy pixel data over from the user provided source data into the OpenGL
  // driver to be used as a texture from now on.
  glTexImage2D(GL_TEXTURE_2D, 0, format, size.width(), size.height(), 0, format,
               GL_UNSIGNED_BYTE, data);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error calling glTexImage2D(size = (" << size.width() << ", "
               << size.height() << "))";
    GL_CALL(glDeleteTextures(1, &texture_handle));
    // 0 is considered by GL to be an invalid handle.
    texture_handle = 0;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  return texture_handle;
}
}  // namespace

TextureDataCPU::TextureDataCPU(const math::Size& size, GLenum format)
    : size_(size),
      format_(format),
      memory_(new uint8_t[GetPitchInBytes() * size_.height()]) {}

GLuint TextureDataCPU::ConvertToTexture(GraphicsContextEGL* graphics_context,
                                        bool bgra_supported) {
  GLuint ret =
      UploadPixelDataToNewTexture(graphics_context, GetMemory(), size_, format_,
                                  GetPitchInBytes(), bgra_supported);
  // Clear out our memory regardless of if the texture creation was successful.
  memory_.reset();

  return ret;
}

bool TextureDataCPU::CreationError() { return memory_.get() == NULL; }

RawTextureMemoryCPU::RawTextureMemoryCPU(size_t size_in_bytes, size_t alignment)
    : size_in_bytes_(size_in_bytes) {
  memory_ = scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree>(
      static_cast<uint8_t*>(base::AlignedAlloc(size_in_bytes, alignment)));
}

GLuint RawTextureMemoryCPU::CreateTexture(GraphicsContextEGL* graphics_context,
                                          intptr_t offset,
                                          const math::Size& size, GLenum format,
                                          int pitch_in_bytes,
                                          bool bgra_supported) const {
  return UploadPixelDataToNewTexture(graphics_context, memory_.get() + offset,
                                     size, format, pitch_in_bytes,
                                     bgra_supported);
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
