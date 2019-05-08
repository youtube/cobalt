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

#include "starboard/common/optional.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

namespace {
struct ConfiguredWindowSurface {
  EGLConfig config;

  EGLSurface surface;
};

// This function will search for an EGLConfig that enables a successful
// eglCreateWindowSurface() and return that config with the created EGLSurface,
// if successful.
starboard::optional<ConfiguredWindowSurface> GetConfiguredWindowSurface(
    EGLDisplay display,
    EGLNativeWindowType native_window) {
  // Query for how many configs match desired attributes.
  EGLint num_configs = 0;
  EGL_CALL(eglChooseConfig(display,
                           starboard::shared::blittergles::kConfigAttributeList,
                           NULL, 0, &num_configs));
  if (num_configs <= 0) {
    SB_DLOG(ERROR) << ": Found no matching configs.";
    return starboard::nullopt;
  }

  std::unique_ptr<EGLConfig[]> configs(new EGLConfig[num_configs]);
  eglChooseConfig(display, starboard::shared::blittergles::kConfigAttributeList,
                  configs.get(), num_configs, &num_configs);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Error getting matching EGLConfigs.";
    return starboard::nullopt;
  }

  // Find first config that allows eglCreateWindowSurface().
  EGLConfig config;
  EGLSurface surface;
  ConfiguredWindowSurface result;
  for (int config_number = 0; config_number < num_configs; ++config_number) {
    config = configs[config_number];
    surface = eglCreateWindowSurface(display, config, native_window, NULL);
    if (eglGetError() == EGL_SUCCESS) {
      result.config = config;
      result.surface = surface;
      return result;
    }
  }

  SB_DLOG(ERROR) << ": Couldn't find a config that allows "
                 << "eglCreateWindowSurface().";
  return starboard::nullopt;
}
}  // namespace

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
  starboard::ScopedLock lock(device->mutex);

  // If there is no config set on the device,
  // SbBlitterCreateSwapChainFromWindow() mutates the device object to set the
  // config.
  if (device->config == NULL) {
    starboard::optional<ConfiguredWindowSurface> configuredWindowSurface =
        GetConfiguredWindowSurface(device->display, native_window);
    if (!configuredWindowSurface) {
      return kSbBlitterInvalidSwapChain;
    }
    device->config = configuredWindowSurface->config;
    swap_chain->surface = configuredWindowSurface->surface;
  } else {
    swap_chain->surface = eglCreateWindowSurface(
        device->display, device->config, native_window, NULL);
    if (eglGetError() != EGL_SUCCESS) {
      SB_DLOG(ERROR) << ": Failed to create an EGLSurface.";
      return kSbBlitterInvalidSwapChain;
    }
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

  return swap_chain.release();
}
