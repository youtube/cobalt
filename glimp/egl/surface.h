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

#include "glimp/base/scoped_ptr.h"
#include "glimp/egl/surface_impl.h"

namespace glimp {
namespace egl {

typedef std::map<int, int> ValidatedSurfaceAttribs;

class Surface {
 public:
  explicit Surface(base::scoped_ptr<SurfaceImpl> surface_impl);

 private:
  base::scoped_ptr<SurfaceImpl> surface_impl_;
};

bool ValidateSurfaceAttribList(const EGLint* raw_attribs,
                               ValidatedSurfaceAttribs* validated_attribs);

EGLSurface ToEGLSurface(Surface* surface);
Surface* FromEGLSurface(EGLSurface surface);

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_SURFACE_H_
