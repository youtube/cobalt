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

SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid device.";
    return kSbBlitterInvalidContext;
  }

  // Create a context and initialize its state to default values.
  SbBlitterContextPrivate* context = new SbBlitterContextPrivate();
  context->device = device;
  context->current_render_target = NULL;
  context->blending_enabled = false;
  context->current_color = SbBlitterColorFromRGBA(255, 255, 255, 255);
  context->modulate_blits_with_color = false;
  context->scissor = SbBlitterMakeRect(0, 0, 0, 0);

  return context;
}
