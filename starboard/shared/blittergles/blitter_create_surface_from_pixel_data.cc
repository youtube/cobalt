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

#include <memory>

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/blitter_surface.h"

SbBlitterSurface SbBlitterCreateSurfaceFromPixelData(
    SbBlitterDevice device,
    SbBlitterPixelData pixel_data) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidSurface;
  }
  if (!SbBlitterIsPixelDataValid(pixel_data)) {
    SB_DLOG(ERROR) << ": Unsupported pixel format.";
    return kSbBlitterInvalidSurface;
  }
  if (device != pixel_data->device) {
    SB_DLOG(ERROR) << ": SbBlitterSurface must be created from the same device "
                   << "that created the SbBlitterPixelData object.";
    return kSbBlitterInvalidSurface;
  }
  if (pixel_data->data == NULL) {
    SB_DLOG(ERROR) << ": No pixel data to copy.";
    return kSbBlitterInvalidSurface;
  }

  std::unique_ptr<SbBlitterSurfacePrivate> surface(
      new SbBlitterSurfacePrivate());
  surface->device = device;
  surface->info.width = pixel_data->width;
  surface->info.height = pixel_data->height;
  surface->info.format =
      SbBlitterPixelDataFormatToSurfaceFormat(pixel_data->format);
  surface->render_target = kSbBlitterInvalidRenderTarget;
  surface->color_texture_handle = 0;
  starboard::shared::blittergles::ChangeDataFormat(
      pixel_data->format, kSbBlitterPixelDataFormatRGBA8,
      SbBlitterGetPixelDataPitchInBytes(pixel_data), pixel_data->height,
      pixel_data->data);
  surface->SetTexture(pixel_data->data);

  SbMemoryDeallocate(pixel_data->data);
  delete pixel_data;
  return surface.release();
}
