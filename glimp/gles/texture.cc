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

#include "glimp/gles/texture.h"

#include "glimp/nb/pointer_arithmetic.h"
#include "glimp/nb/rect.h"

namespace glimp {
namespace gles {

Texture::Texture(nb::scoped_ptr<TextureImpl> impl)
    : impl_(impl.Pass()), target_valid_(false), texture_allocated_(false) {}

void Texture::SetTarget(GLenum target) {
  target_ = target;
  target_valid_ = true;
}

void Texture::SetData(GLint level,
                      PixelFormat pixel_format,
                      GLsizei width,
                      GLsizei height,
                      int pitch_in_bytes,
                      const GLvoid* pixels) {
  width_ = static_cast<int>(width);
  height_ = static_cast<int>(height);
  pixel_format_ = pixel_format;

  impl_->SetData(level, pixel_format_, width_, height_, pitch_in_bytes, pixels);

  texture_allocated_ = true;
}

void Texture::UpdateData(GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLsizei width,
                         GLsizei height,
                         int pitch_in_bytes,
                         const GLvoid* pixels) {
  SB_DCHECK(pixels != NULL);
  impl_->UpdateData(level, nb::Rect<int>(xoffset, yoffset, width, height),
                    pitch_in_bytes, pixels);
}

}  // namespace gles
}  // namespace glimp
