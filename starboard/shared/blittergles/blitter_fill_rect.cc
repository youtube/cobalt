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

namespace {

// Transform the given blitter-space coordinates into NDC.
std::pair<float, float> TransformCoords(
    const int x,
    const int y,
    SbBlitterRenderTargetPrivate* render_target) {
  const float transformed_x = -1 + (2.0f * x) / render_target->width;
  const float transformed_y = 1 - (2.0f * y) / render_target->height;
  return std::pair<float, float>(transformed_x, transformed_y);
}

}  // namespace

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
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
  GL_CALL(glUniform1f(context->blit_uniform, 0.0f));

  std::pair<float, float> lower_left_coord = TransformCoords(
      rect.x, rect.y + rect.height, context->current_render_target);
  std::pair<float, float> upper_right_coord = TransformCoords(
      rect.x + rect.width, rect.y, context->current_render_target);
  float vertices[] = {lower_left_coord.first,  lower_left_coord.second,
                      lower_left_coord.first,  upper_right_coord.second,
                      upper_right_coord.first, lower_left_coord.second,
                      upper_right_coord.first, upper_right_coord.second};
  GL_CALL(glVertexAttribPointer(context->kPositionAttribute, 2, GL_FLOAT,
                                GL_FALSE, 0, vertices));
  GL_CALL(glEnableVertexAttribArray(context->kPositionAttribute));

  const float kColorMapper = 255.0;
  GL_CALL(glVertexAttrib4f(
      context->kColorAttribute,
      SbBlitterRFromColor(context->current_color) / kColorMapper,
      SbBlitterGFromColor(context->current_color) / kColorMapper,
      SbBlitterBFromColor(context->current_color) / kColorMapper,
      SbBlitterAFromColor(context->current_color) / kColorMapper));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  bool success = glGetError() == GL_NO_ERROR;

  GL_CALL(glDisableVertexAttribArray(context->kPositionAttribute));
  return success;
}
