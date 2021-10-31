/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_STUB_GLES_TEXTURE_IMPL_H_
#define GLIMP_STUB_GLES_TEXTURE_IMPL_H_

#include <vector>

#include "glimp/gles/buffer.h"
#include "glimp/gles/pixel_format.h"
#include "glimp/gles/texture_impl.h"
#include "nb/rect.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

class TextureImplStub : public TextureImpl {
 public:
  TextureImplStub();
  ~TextureImplStub() override {}

  void Initialize(int level,
                  PixelFormat pixel_format,
                  int width,
                  int height) override;

  bool UpdateData(int level,
                  const nb::Rect<int>& window,
                  int pitch_in_bytes,
                  const void* pixels) override;

  void UpdateDataFromBuffer(
      int level,
      const nb::Rect<int>& window,
      int pitch_in_bytes,
      const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
      uintptr_t buffer_offset) override;

  void BindToEGLSurface(egl::Surface* surface) override;

  void ReadPixelsAsRGBA8(const nb::Rect<int>& window,
                         int pitch_in_bytes,
                         void* pixels) override;

  bool CanBeAttachedToFramebuffer() const override;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_STUB_GLES_TEXTURE_IMPL_H_
