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

SbBlitterSurface SbBlitterCreateSurfaceFromPixelData(
    SbBlitterDevice device,
    SbBlitterPixelData pixel_data) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return kSbBlitterInvalidSurface;
  }
  if (!SbBlitterIsPixelDataValid(pixel_data)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Unsupported pixel format.";
    return kSbBlitterInvalidSurface;
  }
  if (device != pixel_data->device) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": SbBlitterSurface must be created from "
                   << "the same device that created the SbBlitterPixelData "
                   << "object.";
    return kSbBlitterInvalidSurface;
  }

  // This calls signals that the CPU is done editing pixel data, so unlock the
  // surface that was locked immediately upon creation.
  pixel_data->surface->Unlock(pixel_data->surface);

  // Setup our internal DirectFB surface reference to refer to the DirectFB
  // surface formerly owned by the SbBlitterPixelData object.
  SbBlitterSurfacePrivate* surface = new SbBlitterSurfacePrivate();
  surface->device = pixel_data->device;
  surface->info = pixel_data->info;
  surface->surface = pixel_data->surface;

  // There is no render target for surfaces initilized from pixel data.
  surface->render_target.surface = NULL;

  delete pixel_data;

  return surface;
}
