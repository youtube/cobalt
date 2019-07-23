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

#include "starboard/common/log.h"
#include "starboard/common/recursive_mutex.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

SbBlitterSwapChain SbBlitterCreateSwapChainFromWindow(SbBlitterDevice device,
                                                      SbWindow window) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidSwapChain;
  }
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << ": Invalid window.";
    return kSbBlitterInvalidSwapChain;
  }

  EGLNativeWindowType native_window =
      (EGLNativeWindowType)SbWindowGetPlatformHandle(window);
  std::unique_ptr<SbBlitterSwapChainPrivate> swap_chain(
      new SbBlitterSwapChainPrivate());
  starboard::ScopedRecursiveLock lock(device->mutex);

  swap_chain->surface = eglCreateWindowSurface(device->display, device->config,
                                               native_window, NULL);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to create an EGLSurface.";
    return kSbBlitterInvalidSwapChain;
  }

  eglQuerySurface(device->display, swap_chain->surface, EGL_WIDTH,
                  &swap_chain->render_target.width);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to set render target's width.";
    return kSbBlitterInvalidSwapChain;
  }
  eglQuerySurface(device->display, swap_chain->surface, EGL_HEIGHT,
                  &swap_chain->render_target.height);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to set render target's height.";
    return kSbBlitterInvalidSwapChain;
  }
  swap_chain->render_target.framebuffer_handle = 0;
  swap_chain->render_target.device = device;
  swap_chain->render_target.swap_chain = swap_chain.get();
  swap_chain->render_target.surface = kSbBlitterInvalidSurface;

  return swap_chain.release();
}
