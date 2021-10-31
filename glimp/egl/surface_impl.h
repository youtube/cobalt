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

#ifndef GLIMP_EGL_SURFACE_IMPL_H_
#define GLIMP_EGL_SURFACE_IMPL_H_

#include <EGL/egl.h>

namespace glimp {
namespace egl {

class SurfaceImpl {
 public:
  virtual ~SurfaceImpl() {}

  // Returns a description of the underlying surface.  This method will be
  // referenced when functions like eglQuerySurface() are called.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglQuerySurface.xhtml
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;

  // Returns true if the surface is a window surface, false if the surface is a
  // pixel buffer or a pixmap.
  virtual bool IsWindowSurface() const = 0;

 private:
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_SURFACE_IMPL_H_
