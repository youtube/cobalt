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

#include "starboard/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"

bool SbBlitterSetRenderTarget(SbBlitterContext context,
                              SbBlitterRenderTarget render_target) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }
  if (!SbBlitterIsRenderTargetValid(render_target)) {
    SB_DLOG(ERROR) << ": Invalid render target.";
    return false;
  }

  // TODO: Optimize eglMakeCurrent calls.
  context->current_render_target = render_target;
  if (context->is_current) {
    starboard::ScopedLock lock(context->device->mutex);
    return starboard::shared::blittergles::MakeCurrent(context);
  }

  return true;
}
