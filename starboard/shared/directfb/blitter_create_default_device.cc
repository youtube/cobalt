// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "starboard/log.h"
#include "starboard/shared/directfb/blitter_internal.h"

SbBlitterDevice SbBlitterCreateDefaultDevice() {
  SbBlitterDeviceRegistry* device_registry = GetBlitterDeviceRegistry();
  starboard::ScopedLock lock(device_registry->mutex);

  if (device_registry->default_device) {
    SB_DLOG(ERROR) << __FUNCTION__
                   << ": Default device has already been created.";
    return kSbBlitterInvalidDevice;
  }

  // We only support one DirectFB device, so set it up and return it here.
  IDirectFB* dfb;
  int argc = 0;
  if (DirectFBInit(&argc, NULL) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling DirectFBInit().";
    return kSbBlitterInvalidDevice;
  }

  DirectFBSetOption("mode", "1920x1080");
  DirectFBSetOption("bg-none", NULL);
  DirectFBSetOption("no-cursor", NULL);

  if (DirectFBCreate(&dfb) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling DirectFBCreate().";
    return kSbBlitterInvalidDevice;
  }
  if (dfb->SetCooperativeLevel(dfb, DFSCL_NORMAL) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetCooperativeLevel().";
    dfb->Release(dfb);
    return kSbBlitterInvalidDevice;
  }

  SbBlitterDevicePrivate* device = new SbBlitterDevicePrivate();
  device->dfb = dfb;

  device_registry->default_device = device;

  return device;
}
