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

#include <EGL/egl.h>

#include <memory>

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"

SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidContext;
  }

  starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
      starboard::shared::blittergles::GetBlitterContextRegistry();
  starboard::ScopedLock lock(context_registry->mutex);

  if (context_registry->in_use) {
    SB_DLOG(ERROR) << ": This implementation allows <= 1 SbBlitterContext.";
    return kSbBlitterInvalidContext;
  }

  context_registry->in_use = true;
  return context_registry->context;
}
