// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "starboard/log.h"
#include "starboard/shared/directfb/blitter_internal.h"

bool SbBlitterBlitRectToRect(SbBlitterContext context,
                             SbBlitterSurface source_surface,
                             int src_x,
                             int src_y,
                             int src_width,
                             int src_height,
                             int dst_x,
                             int dst_y,
                             int dst_width,
                             int dst_height) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsSurfaceValid(source_surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid texture.";
    return false;
  }
  if (src_width <= 0 || src_height <= 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Source width and height must both be "
                   << "greater than 0.";
    return false;
  }

  if (dst_width < 0 || dst_height < 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Destination width and height must "
                   << "both be greater than or equal to 0.";
    return false;
  }

  if (!context->current_render_target) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Render target must be previously "
                   << "specified on a context before issuing draw calls.";
    return false;
  }

  IDirectFBSurface* surface = context->current_render_target->surface;

  DFBRectangle source_rect;
  source_rect.x = src_x;
  source_rect.y = src_y;
  source_rect.w = src_width;
  source_rect.h = src_height;

  DFBRectangle dest_rect;
  dest_rect.x = dst_x;
  dest_rect.y = dst_y;
  dest_rect.w = dst_width;
  dest_rect.h = dst_height;

  // Setup the DirectFB blending state on the surface as it is specified in
  // the Blitter API context.
  DFBSurfaceDrawingFlags drawing_flags =
      context->blending_enabled ? DSDRAW_BLEND : DSDRAW_NOFX;
  if (surface->SetDrawingFlags(surface, DSDRAW_BLEND) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetDrawingFlags().";
    return false;
  }

  if (context->modulate_blits_with_color) {
    uint32_t flags =
        DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE;
    if (surface->SetColor(surface, SbBlitterRFromColor(context->current_color),
                          SbBlitterGFromColor(context->current_color),
                          SbBlitterBFromColor(context->current_color),
                          SbBlitterAFromColor(context->current_color)) !=
        DFB_OK) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetColor().";
      return false;
    }

    if (surface->SetBlittingFlags(
            surface, static_cast<DFBSurfaceBlittingFlags>(flags)) != DFB_OK) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetBlittingFlags().";
      return false;
    }
  } else {
    // Setup our DirectFB state to not modulate with color.
    if (surface->SetBlittingFlags(surface, DSBLIT_BLEND_ALPHACHANNEL) !=
        DFB_OK) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetBlittingFlags().";
      return false;
    }
  }

  // Issue the DirectFB draw call to perform the blit.
  if (surface->StretchBlit(surface, source_surface->surface, &source_rect,
                           &dest_rect) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling StretchBlit().";
    return false;
  }

  return true;
}
