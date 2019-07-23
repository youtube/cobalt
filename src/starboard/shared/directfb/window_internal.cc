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

#include "starboard/shared/directfb/window_internal.h"

#include <directfb.h>

#include "starboard/common/log.h"

const int kWindowWidth = 1920;
const int kWindowHeight = 1080;
const int kBPP = 32;

SbWindowPrivate::SbWindowPrivate(IDirectFB* directfb,
                                 const SbWindowOptions* options)
    : directfb(directfb) {
  // Setup the keyboard input device that is represented by this window, as well
  // as an event buffer to direct the keyboard's events towards.
  if (directfb->GetInputDevice(directfb, DIDID_KEYBOARD, &keyboard) != DFB_OK) {
    SB_NOTREACHED() << "Error calling GetInputDevice().";
  }
  if (keyboard->CreateEventBuffer(keyboard, &event_buffer) != DFB_OK) {
    SB_NOTREACHED() << "Error calling CreateEventBuffer().";
  }

  // Setup the window size based on size options.
  int window_width = kWindowWidth;
  int window_height = kWindowHeight;
  if (options && options->size.width > 0 && options->size.height > 0) {
    window_width = options->size.width;
    window_height = options->size.height;
  }
  directfb->SetVideoMode(directfb, window_width, window_height, kBPP);

  width = window_width;
  height = window_height;
}

SbWindowPrivate::~SbWindowPrivate() {
  event_buffer->Release(event_buffer);
  keyboard->Release(keyboard);
}
