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

SbBlitterSurface SbBlitterCreateRenderTargetSurface(
    SbBlitterDevice device,
    int width,
    int height,
    SbBlitterSurfaceFormat surface_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return kSbBlitterInvalidSurface;
  }
  if (!SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(device,
                                                              surface_format)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Unsupported pixel format.";
    return kSbBlitterInvalidSurface;
  }
  if (width <= 0 || height <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Width and height must both be "
                   << "greater than 0.";
    return kSbBlitterInvalidSurface;
  }

  starboard::ScopedLock lock(device->mutex);

  // Create a surface with device-owned memory.
  DFBSurfaceDescription dsc;
  dsc.width = width;
  dsc.height = height;
  dsc.flags = static_cast<DFBSurfaceDescriptionFlags>(
      DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT | DSDESC_CAPS);
  dsc.caps = static_cast<DFBSurfaceCapabilities>(DSCAPS_VIDEOONLY |
                                                 DSCAPS_STATIC_ALLOC);
  dsc.pixelformat = DSPF_ARGB;

  IDirectFB* dfb = device->dfb;
  IDirectFBSurface* directfb_surface;
  if (dfb->CreateSurface(dfb, &dsc, &directfb_surface) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling CreateSurface().";
    return kSbBlitterInvalidSurface;
  }

  SbBlitterSurfacePrivate* surface = new SbBlitterSurfacePrivate();

  // Setup the internal DirectFB surface and have the render target also point
  // to it.
  surface->device = device;
  surface->info.width = width;
  surface->info.height = height;
  surface->info.format = surface_format;
  surface->surface = directfb_surface;
  surface->render_target.surface = directfb_surface;

  return surface;
}
