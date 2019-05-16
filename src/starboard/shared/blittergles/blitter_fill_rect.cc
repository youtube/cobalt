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

#include "starboard/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

bool SbBlitterFillRect(SbBlitterContext context, SbBlitterRect rect) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }
  if (!context->current_render_target) {
    SB_DLOG(ERROR) << ": Context must have a render target set.";
    return false;
  }
  if (rect.width < 0 || rect.height < 0) {
    SB_DLOG(ERROR) << ": Rect must have height and width >= 0.";
    return false;
  }

  starboard::shared::blittergles::ScopedCurrentContext scoped_current_context(
      context);
  if (scoped_current_context.InitializationError()) {
    return false;
  }

  GL_CALL(glEnable(GL_SCISSOR_TEST));

  // SbBlitterRect's x, y are its upper left corner in a coord plane with (0,0)
  // at top left. Here, translate to GL's coord plane of (0,0) at bottom left,
  // and give glScissor() the (x,y) of the rectangle's bottom left corner.
  GL_CALL(glScissor(
      rect.x, context->current_render_target->height - rect.y - rect.height,
      rect.width, rect.height));
  const float kColorMapper = 255.0;
  GL_CALL(
      glClearColor(SbBlitterRFromColor(context->current_color) / kColorMapper,
                   SbBlitterGFromColor(context->current_color) / kColorMapper,
                   SbBlitterBFromColor(context->current_color) / kColorMapper,
                   SbBlitterAFromColor(context->current_color) / kColorMapper));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));

  return true;
}
