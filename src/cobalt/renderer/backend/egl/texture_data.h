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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_

#include <GLES2/gl2.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextEGL;

// All TextureDataEGL subclasses must also implement ConvertToTexture() to
// convert their pixel buffer data to a GL texture object and return the handle
// to it.  After this method is called, the TextureData should be considered
// invalid.
class TextureDataEGL {
 public:
  virtual ~TextureDataEGL() {}

  virtual GLuint ConvertToTexture(GraphicsContextEGL* graphics_context,
                                  bool bgra_supported) = 0;
  // Returns information about the kind of data this TextureData is
  // intended to store.
  virtual const math::Size& GetSize() const = 0;
  virtual GLenum GetFormat() const = 0;

  virtual int GetPitchInBytes() const = 0;

  // Returns a pointer to the image data so that one can set pixel data as
  // necessary.
  virtual uint8_t* GetMemory() = 0;
};

// Similar to TextureDataEGL::ConvertToTexture(), though this creates a texture
// without invalidating the RawTextureMemoryEGL object, so that multiple
// textures may be created through this data.
class RawTextureMemoryEGL {
 public:
  virtual ~RawTextureMemoryEGL() {}

  // Returns the allocated size of the texture memory.
  virtual size_t GetSizeInBytes() const = 0;

  // Returns a CPU-accessible pointer to the allocated memory.
  virtual uint8_t* GetMemory() = 0;

  virtual GLuint CreateTexture(GraphicsContextEGL* graphics_context,
                               intptr_t offset, const math::Size& size,
                               GLenum format, int pitch_in_bytes,
                               bool bgra_supported) const = 0;

 protected:
  // Called within ConstRawTextureMemoryEGL's constructor to indicate that we
  // are
  // done modifying the contained data and that it should be frozen and made
  // immutable at that point.  For example, if using GLES3 PBOs, this would be a
  // good time to call glUnmapBuffer() on the data.
  virtual void MakeConst() = 0;

  friend class ConstRawTextureMemoryEGL;
};

// In order to allow the GPU to access raw texture memory, it must first be
// made immutable by constructing a ConstRawTextureMemoryEGL object from a
// RawTextureMemoryEGL object.  This is so that we can guarantee that when the
// GPU starts reading it, the CPU will not be writing to it and possibly
// creating a race condition.
class ConstRawTextureMemoryEGL
    : public base::RefCountedThreadSafe<ConstRawTextureMemoryEGL> {
 public:
  explicit ConstRawTextureMemoryEGL(
      scoped_ptr<RawTextureMemoryEGL> raw_texture_memory)
      : raw_texture_memory_(raw_texture_memory.Pass()) {
    raw_texture_memory_->MakeConst();
  }

  const RawTextureMemoryEGL& raw_texture_memory() const {
    return *raw_texture_memory_;
  }

  size_t GetSizeInBytes() const {
    return raw_texture_memory_->GetSizeInBytes();
  }

 private:
  virtual ~ConstRawTextureMemoryEGL() {}

  scoped_ptr<RawTextureMemoryEGL> raw_texture_memory_;

  friend class base::RefCountedThreadSafe<ConstRawTextureMemoryEGL>;
};

void SetupInitialTextureParameters();

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_H_
