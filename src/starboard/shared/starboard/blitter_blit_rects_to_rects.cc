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

#include "starboard/blitter.h"
#include "starboard/common/log.h"

// A simple implementation that iterates over each of the incoming rectangles
// and issues a SbBlitterBlitRectToRect() command for each of them.
bool SbBlitterBlitRectsToRects(SbBlitterContext context,
                               SbBlitterSurface source_surface,
                               const SbBlitterRect* src_rects,
                               const SbBlitterRect* dst_rects,
                               int num_rects) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsSurfaceValid(source_surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid texture.";
    return false;
  }

  SbBlitterSurfaceInfo surface_info;
  SbBlitterGetSurfaceInfo(source_surface, &surface_info);

  // Check that our rectangles are valid before proceeding to issue draw calls.
  for (int i = 0; i < num_rects; ++i) {
    if (src_rects[i].width <= 0 || src_rects[i].height <= 0) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Source rectangle (index " << i
                     << ") width and height must both be greater than 0.";
      return false;
    }
    if (dst_rects[i].width < 0 || dst_rects[i].height < 0) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Destination rectangle (index " << i
                     << ") width and height must both be greater than or equal "
                     << "to 0.";
      return false;
    }

    if (src_rects[i].x < 0 || src_rects[i].y < 0 ||
        src_rects[i].x + src_rects[i].width > surface_info.width ||
        src_rects[i].y + src_rects[i].height > surface_info.height) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Source rectangle (index " << i
                     << ") goes out of the source surface's bounds.";
      return false;
    }
  }

  // Now issue the draw calls.
  for (int i = 0; i < num_rects; ++i) {
    if (!SbBlitterBlitRectToRect(context, source_surface, src_rects[i],
                                 dst_rects[i])) {
      return false;
    }
  }

  return true;
}
