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

SbBlitterPixelData SbBlitterCreatePixelData(
    SbBlitterDevice device,
    int width,
    int height,
    SbBlitterPixelDataFormat pixel_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return kSbBlitterInvalidPixelData;
  }
  if (!SbBlitterIsPixelFormatSupportedByPixelData(device, pixel_format)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Unsupported pixel format.";
    return kSbBlitterInvalidPixelData;
  }
  if (width <= 0 || height <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Width and height must both be "
                   << "greater than 0.";
    return kSbBlitterInvalidPixelData;
  }

  starboard::ScopedLock lock(device->mutex);

  // We create an actual DirectFB surface here, and then immediately lock it
  // so that we can grant the CPU access to the pixel data until we are ready
  // to bake it into a SbBlitterSurface.
  IDirectFBSurface* surface;
  DFBSurfaceDescription dsc;
  dsc.width = width;
  dsc.height = height;
  dsc.flags = static_cast<DFBSurfaceDescriptionFlags>(
      DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT | DSDESC_CAPS);
  // Specify that we wish for this surface data to live in device memory.
  dsc.caps = static_cast<DFBSurfaceCapabilities>(DSCAPS_STATIC_ALLOC |
                                                 DSCAPS_PREMULTIPLIED);
  switch (pixel_format) {
// Conversion from Starboard's byte-order color formats to DirectFB's
// word-order color formats.
#if SB_IS_LITTLE_ENDIAN
    case kSbBlitterPixelDataFormatBGRA8: {
#else
    case kSbBlitterPixelDataFormatARGB8: {
#endif
      dsc.pixelformat = DSPF_ARGB;
    } break;
    case kSbBlitterPixelDataFormatA8: {
      dsc.pixelformat = DSPF_A8;
    } break;
    default: { SB_NOTREACHED(); }
  }

  IDirectFB* dfb = device->dfb;
  if (dfb->CreateSurface(dfb, &dsc, &surface) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling CreateSurface().";
    return kSbBlitterInvalidPixelData;
  }

  // Immediately lock the surface so that the CPU has access to the pixel data.
  void* data;
  int pitch_in_bytes;
  if (surface->Lock(surface, DSLF_WRITE, &data, &pitch_in_bytes) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling Lock().";
    surface->Release(surface);
    return kSbBlitterInvalidPixelData;
  }

  SbBlitterPixelDataPrivate* surface_pixels = new SbBlitterPixelDataPrivate();

  surface_pixels->device = device;
  surface_pixels->info.width = width;
  surface_pixels->info.height = height;
  surface_pixels->info.format =
      SbBlitterPixelDataFormatToSurfaceFormat(pixel_format);
  surface_pixels->surface = surface;
  surface_pixels->data = data;
  surface_pixels->pitch_in_bytes = pitch_in_bytes;

  return surface_pixels;
}
