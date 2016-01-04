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

#include "glimp/egl/error.h"
#include "starboard/log.h"

namespace glimp {
namespace egl {

Surface::Surface(nb::scoped_ptr<SurfaceImpl> surface_impl)
    : surface_impl_(surface_impl.Pass()) {}

EGLBoolean Surface::QuerySurface(EGLint attribute, EGLint* value) {
  switch (attribute) {
    case EGL_HEIGHT: {
      *value = surface_impl_->GetHeight();
      return true;
    }

    case EGL_WIDTH: {
      *value = surface_impl_->GetWidth();
      return true;
    }

    case EGL_CONFIG_ID:
    case EGL_HORIZONTAL_RESOLUTION:
    case EGL_LARGEST_PBUFFER:
    case EGL_MIPMAP_LEVEL:
    case EGL_MIPMAP_TEXTURE:
    case EGL_MULTISAMPLE_RESOLVE:
    case EGL_PIXEL_ASPECT_RATIO:
    case EGL_RENDER_BUFFER:
    case EGL_SWAP_BEHAVIOR:
    case EGL_TEXTURE_FORMAT:
    case EGL_TEXTURE_TARGET:
    case EGL_VERTICAL_RESOLUTION: {
      SB_NOTIMPLEMENTED();
    }  // Fall through to default on purpose.

    default:
      SetError(EGL_BAD_ATTRIBUTE);
      return false;
  }

  return true;
}

bool ValidateSurfaceAttribList(const AttribMap& attribs) {
  if (!attribs.empty()) {
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
