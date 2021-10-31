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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/backend/egl/texture_data_cpu.h"

#include "base/memory/aligned_memory.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "starboard/memory.h"

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

  std::unique_ptr<uint8_t, base::AlignedFreeDeleter>
      buffer_for_pitch_adjustment;
  auto width_in_bytes = size.width() * BytesPerPixelForGLFormat(format);
  if (width_in_bytes != pitch_in_bytes) {
    // In case the source image data is not packed tightly, we must reformat it
    // such that the pitch matches the width before passing it to glTexImage2D.
    buffer_for_pitch_adjustment.reset(static_cast<uint8_t*>(
        base::AlignedAlloc(width_in_bytes * size.height(), 8)));
    for (int i = 0; i < size.height(); ++i) {
      memcpy(buffer_for_pitch_adjustment.get() + i * width_in_bytes,
                   data + i * pitch_in_bytes, width_in_bytes);
    }
    data = reinterpret_cast<uint8_t*>(buffer_for_pitch_adjustment.get());
  }

  GLint original_texture_alignment;
  GL_CALL(glGetIntegerv(GL_UNPACK_ALIGNMENT, &original_texture_alignment));
  GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

  // Copy pixel data over from the user provided source data into the OpenGL
  // driver to be used as a texture from now on.
  GL_CALL_SIMPLE(glTexImage2D(GL_TEXTURE_2D, 0, format, size.width(),
                              size.height(), 0, format, GL_UNSIGNED_BYTE,
                              data));
  if (GL_CALL_SIMPLE(glGetError()) != GL_NO_ERROR) {
    LOG(ERROR) << "Error calling glTexImage2D(size = (" << size.width() << ", "
               << size.height() << "))";
    GL_CALL(glDeleteTextures(1, &texture_handle));
    // 0 is considered by GL to be an invalid handle.
    texture_handle = 0;
  }

  GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, original_texture_alignment));

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
  memory_ = std::unique_ptr<uint8_t, base::AlignedFreeDeleter>(
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

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
