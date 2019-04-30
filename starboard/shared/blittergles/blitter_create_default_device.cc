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
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to get EGL display connection.";
    device.reset();
    return kSbBlitterInvalidDevice;
  }

  eglInitialize(device->display, NULL, NULL);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Failed to initialize device.";
    device.reset();
    return kSbBlitterInvalidDevice;
  }

  device->config = NULL;

  device_registry->default_device = device.release();

  return device_registry->default_device;
}
