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

#include <GLES2/gl2.h>

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

bool SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(
    SbBlitterSurface surface,
    SbBlitterPixelDataFormat pixel_format) {
  if (!SbBlitterIsSurfaceValid(surface)) {
    SB_DLOG(ERROR) << ": Invalid surface.";
    return false;
  }

  // glReadPixels() always supports GL_RGBA with type GL_UNSIGNED_BYTE.
  return SbBlitterPixelDataFormatToSurfaceFormat(pixel_format) ==
         kSbBlitterSurfaceFormatRGBA8;
}
