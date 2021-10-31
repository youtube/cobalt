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
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_surface.h"

bool SbBlitterBlitRectToRect(SbBlitterContext context,
                             SbBlitterSurface source_surface,
                             SbBlitterRect src_rect,
                             SbBlitterRect dst_rect) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsSurfaceValid(source_surface)) {
    SB_DLOG(ERROR) << ": Invalid surface.";
    return false;
  }
  if (source_surface->color_texture_handle == 0) {
    SB_DLOG(ERROR) << ": Invalid texture handle on source surface.";
    return false;
  }
  if (!context->current_render_target) {
    SB_DLOG(ERROR) << ": Context must have a render target set.";
    return false;
  }
  if (src_rect.width <= 0 || src_rect.height <= 0) {
    SB_DLOG(ERROR) << ": Source width and height must both be > 0.";
    return false;
  }
  if (dst_rect.width < 0 || dst_rect.height < 0) {
    SB_DLOG(ERROR) << ": Destination width and height must both be >= 0.";
    return false;
  }
  if (src_rect.x < 0 || src_rect.y < 0 ||
      src_rect.x + src_rect.width > source_surface->info.width ||
      src_rect.y + src_rect.height > source_surface->info.height) {
    SB_DLOG(ERROR) << ": Source rectangle goes out of source surface's bounds.";
    return false;
  }
  if (dst_rect.width == 0 || dst_rect.height == 0) {
    // Outputting to a 0-area rectangle. Trivially succeed.
    return true;
  }

  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context(context);
  if (scoped_current_context.InitializationError()) {
    return false;
  }

  context->PrepareDrawState();
  const starboard::shared::blittergles::BlitShaderProgram& blit_shader_program =
      context->GetBlitShaderProgram();
  return blit_shader_program.Draw(context, source_surface, src_rect, dst_rect);
}
