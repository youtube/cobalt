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

bool SbBlitterBlitRectToRect(SbBlitterContext context,
                             SbBlitterSurface source_surface,
                             SbBlitterRect src_rect,
                             SbBlitterRect dst_rect) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsSurfaceValid(source_surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid texture.";
    return false;
  }
  if (!context->current_render_target) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Render target must be previously "
                   << "specified on a context before issuing draw calls.";
    return false;
  }
  if (src_rect.width <= 0 || src_rect.height <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Source width and height must both be "
                   << "greater than 0.";
    return false;
  }
  if (dst_rect.width < 0 || dst_rect.height < 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Destination width and height must "
                   << "both be greater than or equal to 0.";
    return false;
  }
  if (src_rect.x < 0 || src_rect.y < 0 ||
      src_rect.x + src_rect.width > source_surface->info.width ||
      src_rect.y + src_rect.height > source_surface->info.height) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Source rectangle goes out of the "
                   << "source surface's bounds.";
    return false;
  }
  if (dst_rect.width == 0 || dst_rect.height == 0) {
    // Outputting to a 0-area rectangle, so we trivially succeed.
    return true;
  }

  IDirectFBSurface* surface = context->current_render_target->surface;

  DFBRectangle source_rect;
  source_rect.x = src_rect.x;
  source_rect.y = src_rect.y;
  source_rect.w = src_rect.width;
  source_rect.h = src_rect.height;

  DFBRectangle dest_rect;
  dest_rect.x = dst_rect.x;
  dest_rect.y = dst_rect.y;
  dest_rect.w = dst_rect.width;
  dest_rect.h = dst_rect.height;

  if (!SetupDFBSurfaceBlitStateFromBlitterContextState(context, source_surface,
                                                       surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error setting DFB Surface state.";
    return false;
  }

  // Issue the DirectFB draw call to perform the blit.
  if (surface->StretchBlit(surface, source_surface->surface, &source_rect,
                           &dest_rect) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling StretchBlit().";
    return false;
  }

  return true;
}
