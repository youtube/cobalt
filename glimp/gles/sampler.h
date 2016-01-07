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

#ifndef GLIMP_GLES_SAMPLER_H_
#define GLIMP_GLES_SAMPLER_H_

#include <GLES3/gl3.h>

#include "glimp/gles/texture.h"
#include "glimp/nb/ref_counted.h"

namespace glimp {
namespace gles {

// Helper function to represent what GL ES calls an "active texture unit".
// This is what you would select between by calling the glActiveTexture()
// function.  It is called sampler here because this terminology is more widely
// used outside of OpenGL and more clearly disambiguates the function of
// sampler objects and texture objects.
struct Sampler {
  // Initialize the sampler with default values defined by the GL ES
  // specification:
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexParameter.xml
  Sampler()
      : mag_filter(GL_LINEAR),
        min_filter(GL_NEAREST_MIPMAP_LINEAR),
        wrap_s(GL_REPEAT),
        wrap_t(GL_REPEAT) {}

  // Paremters that are modifiable via glTexParameter() functions.
  int mag_filter;
  int min_filter;
  int wrap_s;
  int wrap_t;

  // The currently bound 2D texture, set by calling
  // glBindTexture(GL_TEXTURE_2D) to affect the current sampler (e.g.
  // active texture) slot.
  nb::scoped_refptr<Texture> bound_texture;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_SAMPLER_H_
