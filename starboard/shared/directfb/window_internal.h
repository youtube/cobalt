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

#ifndef STARBOARD_SHARED_DIRECTFB_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_DIRECTFB_WINDOW_INTERNAL_H_

#include <directfb.h>

#include "starboard/shared/internal_only.h"
#include "starboard/window.h"

// The DirectFB Window structure does not own the IDirectFB object (the
// ApplicationDirectFB object owns this), however it does own all input device
// objects and the event buffer corresponding to them.  Thus, when the window
// is destroyed, the input devices and event buffers are shutdown.
struct SbWindowPrivate {
  SbWindowPrivate(IDirectFB* directfb, const SbWindowOptions* options);
  ~SbWindowPrivate();

  // A reference to the IDirectFB object.
  IDirectFB* directfb;

  // A window will create and manage a IDirectFBInputDevice keyboard device.
  IDirectFBInputDevice* keyboard;

  // The event buffer which all input devices will feed into.
  IDirectFBEventBuffer* event_buffer;

  int width;
  int height;
};

#endif  // STARBOARD_SHARED_DIRECTFB_WINDOW_INTERNAL_H_
