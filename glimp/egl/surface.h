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

#ifndef GLIMP_EGL_SURFACE_H_
#define GLIMP_EGL_SURFACE_H_

#include <EGL/egl.h>

#include <map>

#include "glimp/egl/attrib_map.h"
#include "glimp/egl/surface_impl.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace egl {

class Surface {
 public:
  explicit Surface(nb::scoped_ptr<SurfaceImpl> surface_impl);

  int GetWidth() const;
  int GetHeight() const;

  EGLint GetTextureFormat() const;
  EGLint GetTextureTarget() const;

  EGLBoolean QuerySurface(EGLint attribute, EGLint* value);
  EGLBoolean BindTexImage(EGLint buffer);
  EGLBoolean ReleaseTexImage(EGLint buffer);

  bool is_bound_to_texture() const { return is_bound_to_texture_; }

  SurfaceImpl* impl() const { return surface_impl_.get(); }

 private:
  nb::scoped_ptr<SurfaceImpl> surface_impl_;

  // True if this surface is currently bound to a GL ES texture via
  // eglBindTexImage().
  bool is_bound_to_texture_;
};

bool ValidateSurfaceAttribList(const AttribMap& attribs);

EGLSurface ToEGLSurface(Surface* surface);
Surface* FromEGLSurface(EGLSurface surface);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_SURFACE_H_
