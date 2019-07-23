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

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

bool SbBlitterDestroyDevice(SbBlitterDevice device) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Cannot destroy invalid device.";
    return false;
  }

  starboard::shared::blittergles::SbBlitterDeviceRegistry* device_registry =
      starboard::shared::blittergles::GetBlitterDeviceRegistry();
  starboard::ScopedLock lock(device_registry->mutex);

  // Only need to check the default device, because that's currently the only
  // way to create a device in the SbBlitter interface.
  if (device_registry->default_device != device) {
    SB_DLOG(ERROR) << ": Attempted to destroy a device handle that has never "
                   << "been created before.";
    return false;
  }

  starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
      starboard::shared::blittergles::GetBlitterContextRegistry();
  starboard::ScopedLock context_lock(context_registry->mutex);
  delete context_registry->context;
  context_registry->in_use = false;

  // Release all resources associated with device.
  EGL_CALL(eglMakeCurrent(device_registry->default_device->display,
                          EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
  EGLBoolean terminate_result =
      eglTerminate(device_registry->default_device->display);
  if (eglGetError() != EGL_SUCCESS) {
    SB_DLOG(ERROR) << ": Cannot eglTerminate a non-EGLDisplay object.";

    return false;
  }
  if (terminate_result == EGL_FALSE) {
    SB_DLOG(ERROR) << ": Failed to terminate the the device.";
    return false;
  }

  device_registry->default_device = NULL;
  delete device;

  return true;
}
