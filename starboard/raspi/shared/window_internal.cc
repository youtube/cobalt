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

#include "starboard/raspi/shared/window_internal.h"

#include "starboard/common/log.h"

namespace {
const int32_t kLayer = 0;
}  // namespace

using starboard::raspi::shared::DispmanxDisplay;
using starboard::raspi::shared::DispmanxElement;
using starboard::raspi::shared::DispmanxRect;
using starboard::raspi::shared::DispmanxResource;

SbWindowPrivate::SbWindowPrivate(const DispmanxDisplay& display,
                                 const SbWindowOptions* options) {
  uint32_t window_width = 0;
  uint32_t window_height = 0;
  if (options && options->size.width > 0 && options->size.height > 0) {
    window_width = options->size.width;
    window_height = options->size.height;
  } else {
    // Default is the full screen.
    int32_t result =
        graphics_get_display_size(0, &window_width, &window_height);
    SB_DCHECK(result >= 0);
  }

  DispmanxRect destination_rect(0, 0, window_width, window_height);
  // The "<< 16"s are part of the examples, but unexplained. It appears to work.
  DispmanxRect source_rect(0, 0, window_width << 16, window_height << 16);
  // The window doesn't have an image resource associated with it.
  DispmanxResource resource;
  // Creating a window (called an "element" here, created by adding it to the
  // display) must happen within an "update", which seems to represent a sort of
  // window manager transaction.
  element.reset(new DispmanxElement(display, kLayer, destination_rect, resource,
                                    source_rect));
  // We can then populate this struct, a pointer to which is what EGL expects as
  // a "native window" handle.
  window.element = element->handle();
  window.width = window_width;
  window.height = window_height;
}

SbWindowPrivate::~SbWindowPrivate() {
  element.reset();
}
