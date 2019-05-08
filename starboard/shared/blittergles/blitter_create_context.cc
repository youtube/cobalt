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

#include <memory>

#include "starboard/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"

SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidContext;
  }

  std::unique_ptr<SbBlitterContextPrivate> context(
      new SbBlitterContextPrivate());
  context->current_render_target = kSbBlitterInvalidRenderTarget;
  context->device = device;
  context->dummy_surface = EGL_NO_SURFACE;
  context->blending_enabled = false;
  context->current_color = SbBlitterColorFromRGBA(255, 255, 255, 255);
  context->modulate_blits_with_color = false;
  context->scissor = SbBlitterMakeRect(0, 0, 0, 0);
  context->is_current = false;

  // If config hasn't been initialized, we defer creation. It's possible that
  // context will eventually be created with a config that doesn't check for
  // eglCreateWindowSurface() compatibility.
  if (device->config == NULL) {
    context->egl_context = EGL_NO_CONTEXT;
  } else {
    starboard::ScopedLock lock(device->mutex);
    context->egl_context =
        eglCreateContext(device->display, device->config, EGL_NO_CONTEXT,
                         starboard::shared::blittergles::kContextAttributeList);
    if (context->egl_context == EGL_NO_CONTEXT) {
      SB_DLOG(ERROR) << ": Failed to create EGLContext.";
      return kSbBlitterInvalidContext;
    }
  }

  return context.release();
}
