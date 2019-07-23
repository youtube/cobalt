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
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/color_shader_program.h"
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

  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context(context);
  if (scoped_current_context.InitializationError()) {
    return false;
  }
  context->PrepareDrawState();
  const starboard::shared::blittergles::ColorShaderProgram&
      color_shader_program = context->GetColorShaderProgram();
  return color_shader_program.Draw(context->current_render_target,
                                   context->current_rgba, rect);
}
