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

bool SbBlitterIsPixelFormatSupportedByPixelData(
    SbBlitterDevice device,
    SbBlitterPixelDataFormat pixel_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return false;
  }

  if (pixel_format == kSbBlitterPixelDataFormatA8) {
    return true;
  }

// DirectFB supports ARGB but not RGBA.
// Since DirectFB specifies its color formats in word-order, we must swap
// them to cater to the Blitter API's specification of byte-order color
// formats.
#if SB_IS_LITTLE_ENDIAN
  return pixel_format == kSbBlitterPixelDataFormatBGRA8;
#else
  return pixel_format == kSbBlitterPixelDataFormatARGB8;
#endif
}
