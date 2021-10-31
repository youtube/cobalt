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

#include "starboard/common/log.h"

bool SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(
    SbBlitterDevice device,
    SbBlitterSurfaceFormat surface_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return false;
  }

  // In our config attribute list, we set EGL_COLOR_BUFFER_TYPE to
  // EGL_RGB_BUFFER, so only RGBA format OK.
  return surface_format == kSbBlitterSurfaceFormatRGBA8;
}
