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

#include "glimp/stub/gles/texture_impl.h"

#include <algorithm>

#include "glimp/gles/convert_pixel_data.h"
#include "glimp/stub/egl/pbuffer_surface_impl.h"
#include "glimp/stub/gles/buffer_impl.h"
#include "nb/polymorphic_downcast.h"
#include "starboard/memory.h"

namespace glimp {
namespace gles {

TextureImplStub::TextureImplStub() {}

void TextureImplStub::Initialize(int level,
                                 PixelFormat pixel_format,
                                 int width,
                                 int height) {
  SB_DCHECK(level == 0);
}

bool TextureImplStub::UpdateData(int level,
                                 const nb::Rect<int>& window,
                                 int pitch_in_bytes,
                                 const void* pixels) {
  return false;
}

void TextureImplStub::UpdateDataFromBuffer(
    int level,
    const nb::Rect<int>& window,
    int pitch_in_bytes,
    const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
    uintptr_t buffer_offset) {}

void TextureImplStub::BindToEGLSurface(egl::Surface* surface) {}

void TextureImplStub::ReadPixelsAsRGBA8(const nb::Rect<int>& window,
                                        int pitch_in_bytes,
                                        void* pixels) {}

bool TextureImplStub::CanBeAttachedToFramebuffer() const {
  return false;
}

}  // namespace gles
}  // namespace glimp
