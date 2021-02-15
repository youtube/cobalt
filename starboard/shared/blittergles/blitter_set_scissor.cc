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

bool SbBlitterSetScissor(SbBlitterContext context, SbBlitterRect rect) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }
  if (rect.width < 0 || rect.height < 0) {
    SB_DLOG(ERROR) << ": Width and height must both be >= 0.";
    return false;
  }
  if (!SbBlitterIsRenderTargetValid(context->current_render_target)) {
    SB_DLOG(ERROR) << ": Render target must be previously specified on a "
                   << "context before setting the scissor.";
    return false;
  }

  context->scissor = rect;
  return true;
}
