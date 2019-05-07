// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/blitter.h"

#include <EGL/egl.h>

#include "starboard/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"

bool SbBlitterDestroyContext(SbBlitterContext context) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }

  if (context->egl_context != EGL_NO_CONTEXT) {
    starboard::ScopedLock lock(context->device->mutex);

    // For now, we assume context is already unbound, as we bind and unbind
    // context after every Blitter API call that uses it.
    // TODO: Optimize eglMakeCurrent calls, so rebinding is not needed for every
    // API call.
    eglDestroyContext(context->device->display, context->egl_context);
    if (eglGetError() != EGL_SUCCESS) {
      SB_DLOG(ERROR) << ": Failed to destroy egl_context.";
      return false;
    }
  }

  if (context->dummy_surface != EGL_NO_SURFACE) {
    starboard::ScopedLock lock(context->device->mutex);
    eglDestroySurface(context->device->display, context->dummy_surface);
    if (eglGetError() != EGL_SUCCESS) {
      SB_DLOG(ERROR) << ": Failed to destroy dummy_surface.";
      return false;
    }
  }

  delete context;
  return true;
}
