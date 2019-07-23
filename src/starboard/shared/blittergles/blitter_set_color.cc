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
#include "starboard/shared/blittergles/blitter_internal.h"

bool SbBlitterSetColor(SbBlitterContext context, SbBlitterColor color) {
  if (!SbBlitterIsContextValid(context)) {
    SB_DLOG(ERROR) << ": Invalid context.";
    return false;
  }

  const float kColorMapper = 255.0f;
  float alpha = SbBlitterAFromColor(color) / kColorMapper;
  context->current_rgba[0] =
      alpha * (SbBlitterRFromColor(color) / kColorMapper);
  context->current_rgba[1] =
      alpha * (SbBlitterGFromColor(color) / kColorMapper);
  context->current_rgba[2] =
      alpha * (SbBlitterBFromColor(color) / kColorMapper);
  context->current_rgba[3] = alpha;
  return true;
}
