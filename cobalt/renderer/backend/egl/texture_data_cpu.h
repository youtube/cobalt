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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_

#include "base/memory/aligned_memory.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

// Creates a TextreData object where the texture memory is simply CPU-allocated
// (e.g. via malloc) memory.  While not the most efficient technique, it is
// the most compatible.
class TextureDataCPU : public TextureDataEGL {
 public:
  TextureDataCPU(const math::Size& size, GLenum format);

  const math::Size& GetSize() const OVERRIDE { return size_; }
  GLenum GetFormat() const OVERRIDE { return format_; }

  int GetPitchInBytes() const OVERRIDE {
    return size_.width() * BytesPerPixelForGLFormat(format_);
  }

  uint8_t* GetMemory() OVERRIDE { return static_cast<uint8_t*>(memory_.get()); }

  GLuint ConvertToTexture(GraphicsContextEGL* graphics_context,
                          bool bgra_supported);

  bool CreationError() OVERRIDE;

 private:
  math::Size size_;
  GLenum format_;

  scoped_array<uint8_t> memory_;
};

class RawTextureMemoryCPU : public RawTextureMemoryEGL {
 public:
  RawTextureMemoryCPU(size_t size_in_bytes, size_t alignment);

  // Returns the allocated size of the texture memory.
  size_t GetSizeInBytes() const OVERRIDE { return size_in_bytes_; }

  // Returns a CPU-accessible pointer to the allocated memory.
  uint8_t* GetMemory() OVERRIDE { return memory_.get(); }

  GLuint CreateTexture(GraphicsContextEGL* graphics_context, intptr_t offset,
                       const math::Size& size, GLenum format,
                       int pitch_in_bytes, bool bgra_supported) const OVERRIDE;

 protected:
  void MakeConst() OVERRIDE {}

 private:
  size_t size_in_bytes_;

  scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> memory_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_
