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

#ifndef COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_
#define COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_

#include <memory>

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

  const math::Size& GetSize() const override { return size_; }
  GLenum GetFormat() const override { return format_; }

  int GetPitchInBytes() const override {
    return size_.width() * BytesPerPixelForGLFormat(format_);
  }

  uint8_t* GetMemory() override { return static_cast<uint8_t*>(memory_.get()); }

  GLuint ConvertToTexture(GraphicsContextEGL* graphics_context,
                          bool bgra_supported);

  bool CreationError() override;

 private:
  math::Size size_;
  GLenum format_;

  std::unique_ptr<uint8_t[]> memory_;
};

class RawTextureMemoryCPU : public RawTextureMemoryEGL {
 public:
  RawTextureMemoryCPU(size_t size_in_bytes, size_t alignment);

  // Returns the allocated size of the texture memory.
  size_t GetSizeInBytes() const override { return size_in_bytes_; }

  // Returns a CPU-accessible pointer to the allocated memory.
  uint8_t* GetMemory() override { return memory_.get(); }

  GLuint CreateTexture(GraphicsContextEGL* graphics_context, intptr_t offset,
                       const math::Size& size, GLenum format,
                       int pitch_in_bytes, bool bgra_supported) const override;

 protected:
  void MakeConst() override {}

 private:
  size_t size_in_bytes_;

  std::unique_ptr<uint8_t, base::AlignedFreeDeleter> memory_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_TEXTURE_DATA_CPU_H_
