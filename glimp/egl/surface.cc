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
#include "glimp/gles/context.h"
#include "starboard/common/log.h"

namespace glimp {
namespace egl {

Surface::Surface(nb::scoped_ptr<SurfaceImpl> surface_impl)
    : surface_impl_(surface_impl.Pass()), is_bound_to_texture_(false) {}

int Surface::GetWidth() const {
  return surface_impl_->GetWidth();
}

int Surface::GetHeight() const {
  return surface_impl_->GetHeight();
}

EGLint Surface::GetTextureFormat() const {
  return EGL_TEXTURE_RGBA;
}

EGLint Surface::GetTextureTarget() const {
  return EGL_TEXTURE_2D;
}

EGLBoolean Surface::QuerySurface(EGLint attribute, EGLint* value) {
  switch (attribute) {
    case EGL_HEIGHT: {
      *value = GetHeight();
      return true;
    }

    case EGL_WIDTH: {
      *value = GetWidth();
      return true;
    }

    case EGL_TEXTURE_FORMAT: {
      // glimp only supports EGL_TEXTURE_RGBA.
      *value = GetTextureFormat();
      return true;
    }

    case EGL_TEXTURE_TARGET: {
      // glimp only supports EGL_TEXTURE_2D.
      *value = GetTextureTarget();
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
    case EGL_VERTICAL_RESOLUTION: {
      SB_NOTIMPLEMENTED();
    }  // Fall through to default on purpose.

    default:
      SetError(EGL_BAD_ATTRIBUTE);
      return false;
  }

  return true;
}

EGLBoolean Surface::BindTexImage(EGLint buffer) {
  if (buffer != EGL_BACK_BUFFER) {
    SetError(EGL_BAD_MATCH);
    return false;
  }

  if (is_bound_to_texture_) {
    SetError(EGL_BAD_ACCESS);
    return false;
  }

  if (GetTextureTarget() == EGL_NO_TEXTURE) {
    SetError(EGL_BAD_MATCH);
    return false;
  }
  if (GetTextureTarget() != EGL_TEXTURE_2D) {
    SB_NOTIMPLEMENTED() << "glimp does not support binding anything other than "
                           "EGL_TEXTURE_2D.";
    SetError(EGL_BAD_MATCH);
    return false;
  }

  // When this method is called, we should bind to the currently bound active
  // texture in the current GL context.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglBindTexImage.xhtml
  gles::Context* current_context = gles::Context::GetTLSCurrentContext();
  if (current_context == NULL) {
    SB_DLOG(WARNING)
        << "No GL ES context current during call to eglBindTexImage().";
    // This error is non-specified behavior, but seems reasonable.
    SetError(EGL_BAD_CONTEXT);
    return false;
  }

  is_bound_to_texture_ = current_context->BindTextureToEGLSurface(this);
  return is_bound_to_texture_;
}

EGLBoolean Surface::ReleaseTexImage(EGLint buffer) {
  if (buffer != EGL_BACK_BUFFER) {
    SetError(EGL_BAD_MATCH);
    return false;
  }

  if (!is_bound_to_texture_) {
    // Nothing to do if the surface is not already bound.
    return true;
  }

  gles::Context* current_context = gles::Context::GetTLSCurrentContext();
  if (current_context == NULL) {
    SB_DLOG(WARNING)
        << "No GL ES context current during call to eglReleaseTexImage().";
    // This error is non-specified behavior, but seems reasonable.
    SetError(EGL_BAD_CONTEXT);
    return false;
  }

  if (current_context->ReleaseTextureFromEGLSurface(this)) {
    is_bound_to_texture_ = false;
    return true;
  } else {
    return false;
  }
}

namespace {
bool AttributeKeyAndValueAreValid(int key, int value) {
  switch (key) {
    // First deal with the trivial keys where all values are valid.
    case EGL_WIDTH:
    case EGL_HEIGHT: {
      return true;
    }

    case EGL_TEXTURE_TARGET: {
      return value == EGL_TEXTURE_2D;
    }

    case EGL_TEXTURE_FORMAT: {
      return value == EGL_TEXTURE_RGBA;
    }
  }

  // If the switch statement didn't catch the key, this is an unknown
  // key.
  // TODO: glimp doesn't support all values yet, and will return false for keys
  //       that it doesn't support.
  return false;
}
}  // namespace

bool ValidateSurfaceAttribList(const AttribMap& attribs) {
  for (AttribMap::const_iterator iter = attribs.begin(); iter != attribs.end();
       ++iter) {
    if (!AttributeKeyAndValueAreValid(iter->first, iter->second)) {
      return false;
    }
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
