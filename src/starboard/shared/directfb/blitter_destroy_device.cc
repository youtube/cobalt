// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <directfb.h>

#include "starboard/blitter.h"
#include "starboard/common/log.h"
#include "starboard/shared/directfb/blitter_internal.h"

bool SbBlitterDestroyDevice(SbBlitterDevice device) {
  SbBlitterDeviceRegistry* device_registry = GetBlitterDeviceRegistry();
  starboard::ScopedLock lock(device_registry->mutex);

  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Cannot destroy invalid device.";
    return false;
  }

  // Since creation of the default device is currently the only way to create
  // a device in the SbBlitter interface, we only need to check that variable
  // to see if we are deleting a valid device or not.
  if (device_registry->default_device != device) {
    SB_DLOG(ERROR) << __FUNCTION__
                   << ": Attempted to destroy a device handle that has never "
                   << "been created before.";
    return false;
  }

  device_registry->default_device = NULL;
  delete device;

  return true;
}
