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

SbBlitterPixelData SbBlitterCreatePixelData(
    SbBlitterDevice device,
    int width,
    int height,
    SbBlitterPixelDataFormat pixel_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidPixelData;
  }
  if (!SbBlitterIsPixelFormatSupportedByPixelData(device, pixel_format)) {
    SB_DLOG(ERROR) << ": Invalid pixel format.";
    return kSbBlitterInvalidPixelData;
  }
  if (width <= 0 || height <= 0) {
    SB_DLOG(ERROR) << ": Height and width must be > 0.";
    return kSbBlitterInvalidPixelData;
  }

  std::unique_ptr<SbBlitterPixelDataPrivate> pixel_data(
      new SbBlitterPixelDataPrivate());
  pixel_data->device = device;
  pixel_data->width = width;
  pixel_data->height = height;
  pixel_data->format = pixel_format;
  pixel_data->pitch_in_bytes =
      SbBlitterBytesPerPixelForFormat(pixel_format) * width;
  pixel_data->data = SbMemoryAllocate(height * pixel_data->pitch_in_bytes);
  if (pixel_data->data == NULL) {
    SB_DLOG(ERROR) << ": Failed to allocate memory for data.";
    return kSbBlitterInvalidPixelData;
  }

  return pixel_data.release();
}
