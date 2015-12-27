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

#include "glimp/egl/surface.h"

#include "starboard/log.h"

namespace glimp {
namespace egl {

Surface::Surface(base::scoped_ptr<SurfaceImpl> surface_impl)
    : surface_impl_(surface_impl.Pass()) {}

bool ValidateSurfaceAttribList(const EGLint* raw_attribs,
                               ValidatedSurfaceAttribs* validated_attribs) {
  if (raw_attribs) {
    SB_NOTREACHED() << "Support for specifying surface attributes is not "
                    << "supported in glimp.";
  }
  return true;
}

EGLSurface ToEGLSurface(Surface* surface) {
  return reinterpret_cast<EGLSurface>(surface);
}

Surface* FromEGLSurface(EGLSurface surface) {
  return reinterpret_cast<Surface*>(surface);
}

}  // namespace egl
}  // namespace glimp
