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

bool SbBlitterSetRenderTarget(SbBlitterContext context,
                              SbBlitterRenderTarget render_target) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsRenderTargetValid(render_target)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid render target.";
    return false;
  }

  context->current_render_target = render_target;

  // Get the extents of this render target so that we can later setup the
  // scissor rectangle to the extents of this new render target.
  int width, height;
  if (render_target->surface->GetSize(render_target->surface, &width,
                                      &height) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": GetSize() failed.";
    return false;
  }

  // In  DirectFB, draw state is stored within surfaces.  When switching
  // surfaces here, make the appropriate DirectFB calls to reset the surface's
  // state to known values.
  if (render_target->surface->SetPorterDuff(render_target->surface,
                                            DSPD_NONE) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": SetPorderDuff() failed.";
    return false;
  }

  // Finally, set the scissor rectangle to the extents of this render target.
  if (!SbBlitterSetScissor(context, SbBlitterMakeRect(0, 0, width, height))) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": SbBlitterSetScissor() failed.";
    return false;
  }

  return true;
}
