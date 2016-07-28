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

#ifndef GLIMP_GLES_RENDERBUFFER_H_
#define GLIMP_GLES_RENDERBUFFER_H_

#include <GLES3/gl3.h>

#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

// Renderbuffers are objects used to represent non-texture surfaces, such
// as depth buffers or stencil buffers.  They can also be used as color
// render targets but this is not supported by glimp.  Renderbuffers must be
// attached to framebuffer objects via glFramebufferRenderbuffer() in order for
// them to be used.
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGenRenderbuffers.xml
class Renderbuffer : public nb::RefCountedThreadSafe<Renderbuffer> {
 public:
  Renderbuffer();
  void Initialize(GLenum format, GLsizei width, GLsizei height);

  GLenum format() const { return format_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  friend class nb::RefCountedThreadSafe<Renderbuffer>;
  ~Renderbuffer() {}

  GLenum format_;
  int width_;
  int height_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_RENDERBUFFER_H_
