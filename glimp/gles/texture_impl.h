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

#ifndef GLIMP_GLES_TEXTURE_IMPL_H_
#define GLIMP_GLES_TEXTURE_IMPL_H_

#include "glimp/gles/pixel_format.h"
#include "glimp/gles/shader.h"
#include "glimp/nb/ref_counted.h"

namespace glimp {
namespace gles {

class TextureImpl {
 public:
  virtual ~TextureImpl() {}

  // Internally allocates memory for a texture of the specified format, width
  // and height, for the specified mimap level.  If pixels is not NULL, the
  // pixel values contained are to be copied into the newly allocated texture
  // data memory.  This method is called when glTexImage2D() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
  virtual void SetData(int level,
                       PixelFormat pixel_format,
                       int width,
                       int height,
                       const void* pixels) = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_TEXTURE_IMPL_H_
