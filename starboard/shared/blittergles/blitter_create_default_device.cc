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
#include "starboard/common/optional.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"
#include "starboard/shared/x11/application_x11.h"
#include "starboard/window.h"

namespace {

// This function will search for an EGLConfig that enables a successful
// eglCreateWindowSurface(). If there's no existing window, create a best-guess
// config.
starboard::optional<EGLConfig> GetEGLConfig(EGLDisplay display) {
  SbWindow sample_window =
      starboard::shared::x11::ApplicationX11::Get()->GetFirstWindow();
  EGLint num_configs = 0;
  EGLConfig config;
  if (sample_window == kSbWindowInvalid) {
    EGL_CALL(eglChooseConfig(
        display, starboard::shared::blittergles::kConfigAttributeList, &config,
        1, &num_configs));
    if (num_configs == 1) {
      return config;
    }
  } else {
    EGL_CALL(eglChooseConfig(
        display, starboard::shared::blittergles::kConfigAttributeList, NULL, 0,
        &num_configs));
    if (num_configs <= 0) {
      SB_DLOG(ERROR) << ": Found no matching configs.";
      return starboard::nullopt;
    }

    std::unique_ptr<EGLConfig[]> configs(new EGLConfig[num_configs]);
    eglChooseConfig(display,
                    starboard::shared::blittergles::kConfigAttributeList,
                    configs.get(), num_configs, &num_configs);
    if (eglGetError() != EGL_SUCCESS) {
      SB_DLOG(ERROR) << ": Error getting matching EGLConfigs.";
      return starboard::nullopt;
    }

    // Find first config that allows eglCreateWindowSurface().
    EGLSurface surface;
    EGLNativeWindowType native_window =
        (EGLNativeWindowType)SbWindowGetPlatformHandle(sample_window);
    for (int config_number = 0; config_number < num_configs; ++config_number) {
      config = configs[config_number];
      surface = eglCreateWindowSurface(display, config, native_window, NULL);
      if (eglGetError() == EGL_SUCCESS) {
        EGL_CALL(eglDestroySurface(display, surface));
        return config;
      }
    }
  }

  SB_DLOG(ERROR) << ": Couldn't find a compatible config.";
  return starboard::nullopt;
}

}  // namespace

SbBlitterDevice SbBlitterCreateDefaultDevice() {
  starboard::shared::blittergles::SbBlitterDeviceRegistry* device_registry =
      starboard::shared::blittergles::GetBlitterDeviceRegistry();
  starboard::ScopedLock lock(device_registry->mutex);

  if (device_registry->default_device) {
    SB_DLOG(ERROR) << ": Default device has already been created.";
    return kSbBlitterInvalidDevice;
  }

  std::unique_ptr<SbBlitterDevicePrivate> device(new SbBlitterDevicePrivate());
  device->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (device->display == EGL_NO_DISPLAY) {
    SB_DLOG(ERROR) << ": Failed to get EGL display connection.";
    return kSbBlitterInvalidDevice;
  }
  eglInitialize(device->display, NULL, NULL);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to initialize device.";
    return kSbBlitterInvalidDevice;
  }

  starboard::optional<EGLConfig> config = GetEGLConfig(device->display);
  if (!config) {
    return kSbBlitterInvalidDevice;
  }
  device->config = *config;

  device_registry->default_device = device.release();
  std::unique_ptr<SbBlitterContextPrivate> context(
      new SbBlitterContextPrivate(device_registry->default_device));
  if (!context->IsValid()) {
    return kSbBlitterInvalidDevice;
  }
  starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
      starboard::shared::blittergles::GetBlitterContextRegistry();
  starboard::ScopedLock context_lock(context_registry->mutex);
  context_registry->context = context.release();

  return device_registry->default_device;
}
