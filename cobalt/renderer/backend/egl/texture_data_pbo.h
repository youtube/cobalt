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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_PBO_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_PBO_H_

#if defined(GLES3_SUPPORTED)

#include <GLES3/gl3.h>

#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/resource_context.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

// TextureDataPBOs make use of GLES 3.0 GL_PIXEL_UNPACK_BUFFERs to allocate
// pixel memory through the GLES API, giving the driver the opportunity to
// return to us GPU memory (though it must be also accessible via the CPU).
// This allows us to reduce texture uploading when issuing glTexImage2d calls.
class TextureDataPBO : public TextureDataEGL {
 public:
  TextureDataPBO(ResourceContext* resource_context, const math::Size& size,
                 GLenum format);
  virtual ~TextureDataPBO();

  const math::Size& GetSize() const OVERRIDE { return size_; }
  GLenum GetFormat() const OVERRIDE { return format_; }

  int GetPitchInBytes() const OVERRIDE {
    return size_.width() * BytesPerPixelForGLFormat(format_);
  }

  uint8_t* GetMemory() OVERRIDE { return mapped_data_; }

  GLuint ConvertToTexture(GraphicsContextEGL* graphics_context,
                          bool bgra_supported);

  bool CreationError() OVERRIDE;

 private:
  // Private methods that are intended to run only on the resource context
  // thread.
  void InitAndMapPBO();
  void UnmapAndDeletePBO();

  ResourceContext* resource_context_;
  math::Size size_;
  GLenum format_;
  GLuint pixel_buffer_;
  int64 data_size_;
  GLubyte* mapped_data_;
  bool error_;
};

// Similar to TextureDataPBO, but this allows a bit more flexibility and less
// structure in the memory that is allocated, though it still is allocated as
// pixel buffer data (using GL_PIXEL_UNPACK_BUFFER).
class RawTextureMemoryPBO : public RawTextureMemoryEGL {
 public:
  RawTextureMemoryPBO(ResourceContext* resource_context, size_t size_in_bytes,
                      size_t alignment);
  virtual ~RawTextureMemoryPBO();

  // Returns the allocated size of the texture memory.
  size_t GetSizeInBytes() const OVERRIDE { return size_in_bytes_; }

  // Returns a CPU-accessible pointer to the allocated memory.
  uint8_t* GetMemory() OVERRIDE { return mapped_data_ + alignment_offset_; }

  GLuint CreateTexture(GraphicsContextEGL* graphics_context, intptr_t offset,
                       const math::Size& size, GLenum format,
                       int pitch_in_bytes, bool bgra_supported) const OVERRIDE;

 protected:
  void MakeConst() OVERRIDE;

 private:
  // Private methods that are intended to run only on the resource context
  // thread.
  void InitAndMapPBO();
  void DestroyPBO();
  void UnmapPBO();

  size_t size_in_bytes_;
  size_t alignment_;
  ResourceContext* resource_context_;
  GLuint pixel_buffer_;
  GLubyte* mapped_data_;

  // The offset from the beginning of the mapped memory necessary to achieve
  // the desired alignment.
  intptr_t alignment_offset_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // defined(GLES3_SUPPORTED)

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_PBO_H_
