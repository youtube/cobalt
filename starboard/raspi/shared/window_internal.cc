// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <bcm_host.h>

#include "starboard/log.h"

namespace {
const int32_t kLayer = 0;
const DISPMANX_RESOURCE_HANDLE_T kResource = DISPMANX_NO_HANDLE;
}  // namespace

SbWindowPrivate::SbWindowPrivate(DISPMANX_DISPLAY_HANDLE_T display,
                                 const SbWindowOptions* options)
    : display(display), element(DISPMANX_NO_HANDLE) {
  VC_RECT_T destination_rect;
  VC_RECT_T source_rect;

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

  destination_rect.x = 0;
  destination_rect.y = 0;
  destination_rect.width = window_width;
  destination_rect.height = window_height;

  source_rect.x = 0;
  source_rect.y = 0;
  // This shift is part of the examples, but unexplained. It appears to work.
  source_rect.width = window_width << 16;
  source_rect.height = window_height << 16;

  // Creating a window (called an "element" here, created by adding it to the
  // display) must happen within an "update", which seems to represent a sort of
  // window manager transaction.
  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0 /*screen*/);
  SB_DCHECK(update != DISPMANX_NO_HANDLE);
  element = vc_dispmanx_element_add(update, display, kLayer, &destination_rect,
                                    kResource, &source_rect,
                                    DISPMANX_PROTECTION_NONE, NULL /*alpha*/,
                                    NULL /*clamp*/, DISPMANX_NO_ROTATE);
  SB_DCHECK(element != DISPMANX_NO_HANDLE);
  int32_t result = vc_dispmanx_update_submit_sync(update);
  SB_DCHECK(result == 0) << " result=" << result;

  // We can then populate this struct, a pointer to which is what EGL expects as
  // a "native window" handle.
  window.element = element;
  window.width = window_width;
  window.height = window_height;
}

SbWindowPrivate::~SbWindowPrivate() {
  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0 /*screen*/);
  int32_t result = vc_dispmanx_element_remove(update, element);
  SB_DCHECK(result == 0) << " result=" << result;
  vc_dispmanx_update_submit_sync(update);
  element = DISPMANX_NO_HANDLE;
}
